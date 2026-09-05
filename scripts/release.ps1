#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Automated release workflow for Amnesia Head Tracking.
.DESCRIPTION
    Updates version in src/version.h, commits, tags, and pushes to trigger CI release.
    `pixi run release nightly` delegates to release-nightly.ps1 (rolling dev pre-release).
.NOTES
    Run via: pixi run release <major|minor|patch|nightly|X.Y.Z>
#>
param(
    [Parameter(Position=0)]
    [string]$Version = "",
    # Ship a release even when there are no user-facing commits since the
    # last tag (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIPs, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so a bump with no notices edit stopped the release
# here, or in CI once the tag had already been pushed. Re-sync it and let this
# release carry the correction.
$noticesRoot = Split-Path -Parent $PSScriptRoot
& git -C $noticesRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) { throw "THIRD-PARTY-NOTICES.md has uncommitted edits. Commit or discard them, then re-run." }
& (Join-Path $noticesRoot 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $noticesRoot
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $noticesRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $noticesRoot commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir
$versionHeader = Join-Path $projectDir "src\version.h"

# Nightly dispatch.
if ($Version -eq 'nightly') {
    & (Join-Path $scriptDir 'release-nightly.ps1')
    exit $LASTEXITCODE
}

Import-Module (Join-Path $projectDir "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = Get-Content $Path -Raw
    if ($changelog -match '(?s)(# Changelog.*?)(## \[)') {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    } else {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n)', "`$1$entry"
    }
    $changelog = $changelog.TrimEnd() + "`n"
    Set-Content $Path $changelog -NoNewline
}

function Get-CurrentVersion {
    $content = Get-Content $versionHeader -Raw
    $major = [regex]::Match($content, 'VERSION_MAJOR\s*=\s*(\d+)').Groups[1].Value
    $minor = [regex]::Match($content, 'VERSION_MINOR\s*=\s*(\d+)').Groups[1].Value
    $patch = [regex]::Match($content, 'VERSION_PATCH\s*=\s*(\d+)').Groups[1].Value
    return "$major.$minor.$patch"
}

function Set-Version {
    param([string]$NewVersion)
    $parts = $NewVersion.Split('.')
    $content = Get-Content $versionHeader -Raw
    $content = $content -replace 'VERSION_MAJOR\s*=\s*\d+', "VERSION_MAJOR = $($parts[0])"
    $content = $content -replace 'VERSION_MINOR\s*=\s*\d+', "VERSION_MINOR = $($parts[1])"
    $content = $content -replace 'VERSION_PATCH\s*=\s*\d+', "VERSION_PATCH = $($parts[2])"
    $content = $content -replace 'VERSION_STRING\s*=\s*"[^"]*"', "VERSION_STRING = `"$NewVersion`""
    Set-Content $versionHeader $content -NoNewline
}

Write-Host "=== Amnesia Head Tracking Release ===" -ForegroundColor Cyan
Write-Host ""

$currentVersion = Get-CurrentVersion

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "Current version: $currentVersion" -ForegroundColor White
    Write-Host "Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>" -ForegroundColor Yellow
    exit 0
}

try {
    $Version = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

$tagName = "v$Version"

$currentBranch = git rev-parse --abbrev-ref HEAD
if ($currentBranch -ne "main") {
    Write-Host "Error: Must be on 'main' branch to release (currently on '$currentBranch')" -ForegroundColor Red
    exit 1
}

if (git status --porcelain) {
    Write-Host "Error: Working directory has uncommitted changes" -ForegroundColor Red
    exit 1
}

if (git tag -l $tagName) {
    Write-Host "Error: Tag '$tagName' already exists" -ForegroundColor Red
    exit 1
}

Write-Host "Current version: $currentVersion" -ForegroundColor Gray
Write-Host "New version:     $Version" -ForegroundColor Green
Write-Host ""

# Generate CHANGELOG from commits since last tag. This is the gate that
# aborts when there are no user-facing commits, so run it BEFORE mutating
# any version files or building - a failure here then leaves a clean tree
# instead of stranding a half-applied version bump with no tag.
Write-Host "Generating CHANGELOG from commits..." -ForegroundColor Cyan
$changelogPath = Join-Path $projectDir "CHANGELOG.md"
$hasExistingTags = git tag -l 2>$null
if (-not $hasExistingTags) {
    $date = Get-Date -Format 'yyyy-MM-dd'
    Set-Content $changelogPath "# Changelog`n`n## [$Version] - $date`n`nFirst release.`n"
} else {
    try {
        $changelogArgs = @{
            ChangelogPath = $changelogPath
            Version = $Version
            ArtifactPaths = @("src/", "cameraunlock-core/", "scripts/install.cmd", "scripts/uninstall.cmd")
        }
        New-ChangelogFromCommits @changelogArgs
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "No user-facing changes to release. Re-run with -Force for a maintenance release." -ForegroundColor Yellow
            exit 1
        }
        Write-Host "No user-facing commits since last tag - writing maintenance entry (-Force)." -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path $changelogPath -NewVersion $Version
    }
}

Write-Host "Updating version to $Version..." -ForegroundColor Cyan
Set-Version $Version

# Keep install.cmd MOD_VERSION in lockstep.
$installCmdPath = Join-Path $scriptDir "install.cmd"
(Get-Content $installCmdPath -Raw) -replace 'set "MOD_VERSION=.*?"', "set `"MOD_VERSION=$Version`"" | Set-Content $installCmdPath -NoNewline

Write-Host "Building Release configuration..." -ForegroundColor Cyan
& pixi run build-release
if ($LASTEXITCODE -ne 0) { Write-Host "Error: build-release failed" -ForegroundColor Red; exit 1 }

Write-Host "Committing version change..." -ForegroundColor Cyan
git add $versionHeader $changelogPath $installCmdPath
git commit -m "Release v$Version"
git tag $tagName
git push origin main
git push origin $tagName

Write-Host ""
Write-Host "Release $tagName initiated. Watch CI at:" -ForegroundColor Green
Write-Host "  https://github.com/itsloopyo/amnesia-the-dark-descent-headtracking/actions" -ForegroundColor Cyan

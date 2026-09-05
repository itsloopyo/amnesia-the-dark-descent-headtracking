#!/usr/bin/env pwsh
#Requires -Version 5.1
# Packaging for Amnesia Head Tracking (C++ ASI mod). Produces:
#   - AmnesiaHeadTracking-v{version}-installer.zip (GitHub: install.cmd + plugins/ + vendor/ + shared/ + docs)
#   - AmnesiaHeadTracking-v{version}-nexus.zip     (Nexus: extract-to-game-folder layout)
# Offline: consumes whatever is committed under vendor/. Never hits the network.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

function Get-VersionFromHeader {
    $versionHeader = "src/version.h"
    if (-not (Test-Path $versionHeader)) { Write-Host "ERROR: version.h not found" -ForegroundColor Red; exit 1 }
    $content = Get-Content $versionHeader -Raw
    $major = [regex]::Match($content, 'VERSION_MAJOR\s*=\s*(\d+)').Groups[1].Value
    $minor = [regex]::Match($content, 'VERSION_MINOR\s*=\s*(\d+)').Groups[1].Value
    $patch = [regex]::Match($content, 'VERSION_PATCH\s*=\s*(\d+)').Groups[1].Value
    if (-not $major -or -not $minor -or -not $patch) { Write-Host "ERROR: Could not parse version.h" -ForegroundColor Red; exit 1 }
    return "$major.$minor.$patch"
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir
$releaseDir = Join-Path $projectDir "release"

Import-Module (Join-Path $projectDir "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

$version = Get-VersionFromHeader
Write-Host "=== Amnesia Head Tracking - Package Release ===" -ForegroundColor Magenta
Write-Host "Version: $version" -ForegroundColor Cyan
Write-Host ""

$asiPath = Join-Path $projectDir "bin\Release\AmnesiaHeadTracking.asi"
if (-not (Test-Path $asiPath)) { throw "Build output not found: $asiPath (run 'pixi run build-release')" }

$iniPath = Join-Path $projectDir "config\HeadTracking.ini"
if (-not (Test-Path $iniPath)) { throw "Config not found: $iniPath" }

$vendorAsiDir = Join-Path $projectDir "vendor\ultimate-asi-loader"
$vendorAsiDll = Join-Path $vendorAsiDir "dinput8.dll"
if (-not (Test-Path $vendorAsiDll)) { throw "Bundled ASI loader missing: $vendorAsiDll (run 'pixi run update-deps')" }

# Both ZIPs redistribute that binary, and the upstream x86 loader carries
# binkw32.dll (RAD Game Tools, proprietary), wndmode.dll and vorbisfile.dll as
# RCDATA resources. None of the three is ours to ship, so a loader that still
# has them never reaches a release. See vendor/ultimate-asi-loader/README.md.
& (Join-Path $scriptDir "strip-loader-payload.ps1") -Path $vendorAsiDll -VerifyOnly

$scriptsDir = Join-Path $projectDir "scripts"
foreach ($s in @("install.cmd", "uninstall.cmd")) {
    if (-not (Test-Path (Join-Path $scriptsDir $s))) { throw "Required script not found: $s" }
}

$manifestPath = Join-Path $projectDir "launcher-manifest.json"
if (-not (Test-Path $manifestPath)) { throw "launcher-manifest.json not found: $manifestPath" }

if (-not (Test-Path $releaseDir)) { New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null }

# --- GitHub installer ZIP ---
Write-Host "--- GitHub Release ZIP ---" -ForegroundColor Yellow
$ghStaging = Join-Path $releaseDir "staging-github"
if (Test-Path $ghStaging) { Remove-Item -Recurse -Force $ghStaging }
New-Item -ItemType Directory -Path $ghStaging -Force | Out-Null

foreach ($s in @("install.cmd", "uninstall.cmd")) {
    Copy-Item (Join-Path $scriptsDir $s) -Destination $ghStaging -Force
    Write-Host "  $s" -ForegroundColor Green
}

# Stamp the launcher manifest with the real release version and place it at the
# installer ZIP root. The launcher reads this file (delivery_mode is install_cmd,
# so install.cmd still drives the actual install - the manifest is the metadata
# lopari ingests for detection, audit, and future native deployment).
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.mod_info.version = $version
$manifest | ConvertTo-Json -Depth 10 | Set-Content -Path (Join-Path $ghStaging "launcher-manifest.json") -Encoding utf8
Write-Host "  launcher-manifest.json (version $version)" -ForegroundColor Green

$pluginsDir = Join-Path $ghStaging "plugins"
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host "  plugins/AmnesiaHeadTracking.asi" -ForegroundColor Green
Copy-Item $iniPath -Destination $pluginsDir -Force
Write-Host "  plugins/HeadTracking.ini" -ForegroundColor Green

$ghVendorDir = Join-Path $ghStaging "vendor\ultimate-asi-loader"
New-Item -ItemType Directory -Path $ghVendorDir -Force | Out-Null
foreach ($vf in @("dinput8.dll", "LICENSE", "README.md")) {
    $src = Join-Path $vendorAsiDir $vf
    if (-not (Test-Path $src)) { throw "Vendored ASI loader file missing: $vf (its LICENSE must ship with the binary)" }
    Copy-Item $src -Destination $ghVendorDir -Force
    Write-Host "  vendor/ultimate-asi-loader/$vf" -ForegroundColor Green
}

foreach ($doc in @("README.md", "LICENSE", "CHANGELOG.md", "THIRD-PARTY-NOTICES.md")) {
    $docPath = Join-Path $projectDir $doc
    if (-not (Test-Path $docPath)) { throw "Required document missing from the release: $doc" }
    Copy-Item $docPath -Destination $ghStaging -Force
    Write-Host "  $doc" -ForegroundColor Green
}

# MinHook + HDE are BSD-2-Clause and are compiled into the .asi, so their
# notice must ship with the binary. THIRD-PARTY-NOTICES.md reproduces it, and
# the upstream file goes alongside verbatim.
Copy-Item (Join-Path $projectDir "extern\minhook\LICENSE.txt") `
          -Destination (Join-Path $ghStaging "LICENSE-MinHook.txt") -Force
Write-Host "  LICENSE-MinHook.txt" -ForegroundColor Green

Copy-SharedBundle -StagingDir $ghStaging -CoreRoot (Join-Path $projectDir 'cameraunlock-core')

$ghZip = Join-Path $releaseDir "AmnesiaHeadTracking-v$version-installer.zip"
if (Test-Path $ghZip) { Remove-Item $ghZip -Force }
Push-Location $ghStaging
try { Compress-Archive -Path ".\*" -DestinationPath $ghZip -Force } finally { Pop-Location }
Remove-Item -Recurse -Force $ghStaging
Write-Host ("  $ghZip ({0:N1} KB)" -f ((Get-Item $ghZip).Length / 1KB)) -ForegroundColor Green

# --- Nexus ZIP (extract to game folder; Amnesia.exe is at the root) ---
Write-Host ""
Write-Host "--- Nexus Mods ZIP ---" -ForegroundColor Yellow
$nexusStaging = Join-Path $releaseDir "staging-nexus"
if (Test-Path $nexusStaging) { Remove-Item -Recurse -Force $nexusStaging }
New-Item -ItemType Directory -Path $nexusStaging -Force | Out-Null

Copy-Item $asiPath -Destination $nexusStaging -Force
Write-Host "  AmnesiaHeadTracking.asi" -ForegroundColor Green
Copy-Item $iniPath -Destination $nexusStaging -Force
Write-Host "  HeadTracking.ini" -ForegroundColor Green
# Ship the loader pre-named as wininet.dll so Nexus users (no install.cmd) get it loaded.
Copy-Item $vendorAsiDll -Destination (Join-Path $nexusStaging "wininet.dll") -Force
Write-Host "  wininet.dll (Ultimate ASI Loader, x86, MIT)" -ForegroundColor Green

# Licence notices travel with the binaries. The .asi has MinHook and the Hacker
# Disassembler Engine (both BSD-2-Clause) compiled in, and wininet.dll is
# ThirteenAG's MIT-licensed loader; all three require their notice to accompany
# a binary distribution, and the Nexus ZIP has no install.cmd to carry it.
foreach ($doc in @("LICENSE", "THIRD-PARTY-NOTICES.md")) {
    Copy-Item (Join-Path $projectDir $doc) -Destination $nexusStaging -Force
    Write-Host "  $doc" -ForegroundColor Green
}
Copy-Item (Join-Path $vendorAsiDir "LICENSE") `
          -Destination (Join-Path $nexusStaging "LICENSE-Ultimate-ASI-Loader.txt") -Force
Write-Host "  LICENSE-Ultimate-ASI-Loader.txt" -ForegroundColor Green
Copy-Item (Join-Path $projectDir "extern\minhook\LICENSE.txt") `
          -Destination (Join-Path $nexusStaging "LICENSE-MinHook.txt") -Force
Write-Host "  LICENSE-MinHook.txt" -ForegroundColor Green

$nexusZip = Join-Path $releaseDir "AmnesiaHeadTracking-v$version-nexus.zip"
if (Test-Path $nexusZip) { Remove-Item $nexusZip -Force }
# The Nexus ZIP is a binary distribution too: the licences of everything
# compiled into or bundled with the payload require their notices to travel
# with it, so LICENSE and THIRD-PARTY-NOTICES.md ship at its root.
foreach ($noticeDoc in @('LICENSE', 'THIRD-PARTY-NOTICES.md', 'README.md')) {
    $noticeSrc = Join-Path $projectDir $noticeDoc
    if (-not (Test-Path $noticeSrc)) {
        throw "Required notice file not found: $noticeDoc. Every published ZIP is a binary distribution and must carry it."
    }
    Copy-Item $noticeSrc -Destination $nexusStaging -Force
    Write-Host "  $noticeDoc" -ForegroundColor Green
}
Push-Location $nexusStaging
try { Compress-Archive -Path ".\*" -DestinationPath $nexusZip -Force } finally { Pop-Location }
Remove-Item -Recurse -Force $nexusStaging
Write-Host ("  $nexusZip ({0:N1} KB)" -f ((Get-Item $nexusZip).Length / 1KB)) -ForegroundColor Green

Write-Host ""
Write-Host "=== Package Complete ===" -ForegroundColor Magenta
Write-Output $ghZip
Write-Output $nexusZip

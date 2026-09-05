[CmdletBinding()]
param([switch]$AllowDirty)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

# Version comes from src/version.h.
$versionHeader = Join-Path $ProjectRoot 'src\version.h'
$content = Get-Content $versionHeader -Raw
$major = [regex]::Match($content, 'VERSION_MAJOR\s*=\s*(\d+)').Groups[1].Value
$minor = [regex]::Match($content, 'VERSION_MINOR\s*=\s*(\d+)').Groups[1].Value
$patch = [regex]::Match($content, 'VERSION_PATCH\s*=\s*(\d+)').Groups[1].Value
$version = "$major.$minor.$patch"

Publish-NightlyBuild `
    -ModId 'amnesia-the-dark-descent' `
    -ModName 'AmnesiaHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -BuildCommand 'pixi run build-release' `
    -PackageCommand 'pixi run package' `
    -AllowDirty:$AllowDirty

# Deploy Amnesia Head Tracking (.asi + ini) next to Amnesia.exe.
# Usage: scripts/deploy.ps1 [Debug|Release]

param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectRoot "cameraunlock-core\powershell\GamePathDetection.psm1") -Force

$gameId = 'amnesia-the-dark-descent'
$config = Get-GameConfig -GameId $gameId

$gamePath = Find-GamePath -GameId $gameId
if (-not $gamePath) {
    Write-GameNotFoundError -GameName 'Amnesia: The Dark Descent' -EnvVar $config.EnvVar -SteamFolder $config.SteamFolder
    exit 1
}
Write-Host "Game found at: $gamePath" -ForegroundColor Green

# Amnesia.exe sits at the game root, so EXE_DIR is the game path.
$exeDir = Split-Path -Parent (Join-Path $gamePath $config.Executable)

$asiSource = Join-Path $projectRoot "bin\$Configuration\AmnesiaHeadTracking.asi"
$iniSource = Join-Path $projectRoot "config\HeadTracking.ini"

if (-not (Test-Path $asiSource)) {
    Write-Host "ERROR: AmnesiaHeadTracking.asi not found at $asiSource" -ForegroundColor Red
    Write-Host "Run 'pixi run build-release' first." -ForegroundColor Yellow
    exit 1
}
if (-not (Test-Path $iniSource)) {
    Write-Host "ERROR: HeadTracking.ini not found at $iniSource" -ForegroundColor Red
    exit 1
}

# Ensure the ASI loader is present (dropped in as wininet.dll, which Amnesia imports).
$loaderDest = Join-Path $exeDir "wininet.dll"
if (-not (Test-Path $loaderDest)) {
    $vendorDll = Join-Path $projectRoot "vendor\ultimate-asi-loader\dinput8.dll"
    if (-not (Test-Path $vendorDll)) {
        Write-Host "ERROR: Vendored ASI loader missing at $vendorDll. Run 'pixi run update-deps'." -ForegroundColor Red
        exit 1
    }
    Copy-Item $vendorDll -Destination $loaderDest -Force
    Write-Host "Installed ASI loader -> $loaderDest" -ForegroundColor Cyan
} else {
    Write-Host "ASI loader already present at $loaderDest" -ForegroundColor Gray
}

Copy-Item $asiSource -Destination (Join-Path $exeDir "AmnesiaHeadTracking.asi") -Force
Write-Host "Copied AmnesiaHeadTracking.asi -> $exeDir" -ForegroundColor Green

# Don't clobber an existing user config on redeploy.
$iniDest = Join-Path $exeDir "HeadTracking.ini"
if (-not (Test-Path $iniDest)) {
    Copy-Item $iniSource -Destination $iniDest -Force
    Write-Host "Copied HeadTracking.ini -> $exeDir" -ForegroundColor Green
} else {
    Write-Host "HeadTracking.ini already present (left as-is)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Deployment successful. Launch Amnesia to use head tracking." -ForegroundColor Green

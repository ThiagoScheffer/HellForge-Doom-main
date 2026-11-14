param(
    [string]$Iwad = 'F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/DOOM2.WAD',
    [string]$Wad = 'F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/oku2v31.wad',
    [switch]$Profile
)
$ErrorActionPreference = 'Stop'
$engine = Join-Path $PSScriptRoot 'doom.exe'
foreach ($resource in @($engine, $Iwad, $Wad)) {
    if (!(Test-Path -LiteralPath $resource)) { throw "Missing test resource: $resource" }
}
$profileDir = Join-Path $PSScriptRoot 'TestProfile'
New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
$graphics = Join-Path $profileDir 'graphics.cfg'
if (!(Test-Path -LiteralPath $graphics)) {
    @('monster_hibernation 1', 'swarm_debug 1', 'fullscreen 0') | Set-Content -LiteralPath $graphics -Encoding ascii
}
$logDir = Join-Path $PSScriptRoot ('Logs/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$arguments = @('-iwad', $Iwad, '-file', $Wad, '-warp', '1', '-skill', '4',
    '-nodigging', '-profilespikes', '-graphics', $graphics, '-config', (Join-Path $profileDir 'doom.cfg'))
if ($Profile) { $arguments += @('-profilemonsters', (Join-Path $logDir 'Probes')) }
$oldCache = $env:HELLFORGE_MAP_CACHE
$oldCrash = $env:HELLFORGE_CRASH_DIR
$env:HELLFORGE_MAP_CACHE = Join-Path $PSScriptRoot 'Cache'
$env:HELLFORGE_CRASH_DIR = Join-Path $logDir 'Crashes'
Write-Host 'Experimental oku MAP01. Cold lighting preparation can take around 20 seconds.'
Write-Host "Diagnostic log: $(Join-Path $logDir 'engine.log')"
if ($Profile) { Write-Host 'Hot-path profiling enabled; this adds measurable overhead.' }
Push-Location $PSScriptRoot
$nativeErrorAction = $ErrorActionPreference
$engineExit = 1
try {
    # Windows PowerShell can wrap native stderr diagnostics as error records.
    # Keep capturing those records without terminating a healthy game launch.
    $ErrorActionPreference = 'Continue'
    & $engine @arguments *> (Join-Path $logDir 'engine.log')
    $engineExit = $LASTEXITCODE
    Write-Host "Engine exit code: $engineExit"
} finally {
    $ErrorActionPreference = $nativeErrorAction
    Pop-Location
    $env:HELLFORGE_MAP_CACHE = $oldCache
    $env:HELLFORGE_CRASH_DIR = $oldCrash
}
exit $engineExit

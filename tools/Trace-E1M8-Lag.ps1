param([string]$Iwad, [switch]$ValidateOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$launcher = Join-Path $root 'dist\DOOM-Hellforge.exe'
if (!(Test-Path -LiteralPath $launcher)) { throw "Build the launcher first: $launcher" }
if (!$Iwad) { $Iwad = Join-Path $root 'test-references\DOOM.WAD' }
$Iwad = (Resolve-Path -LiteralPath $Iwad).Path

# Reuse the release launcher's product identity and argument builder. Keep its
# bundled integrations, solid-world mode and persistent user settings intact.
$assembly = [Reflection.Assembly]::LoadFile($launcher)
$data = $assembly.GetType('Data', $true)
$flags = [Reflection.BindingFlags]'NonPublic,Static'
$launcherHome = $data.GetField('Home', $flags).GetValue($null)
$profile = $data.GetField('Profile', $flags).GetValue($null)
$graphics = Join-Path $profile 'graphics.cfg'
if (!(Test-Path -LiteralPath $graphics)) { throw 'Run the normal launcher once and apply your F4 settings first.' }
foreach ($name in @('Engine','BundledArguments')) {
    if (!$data.GetMethod($name, $flags)) { throw "Launcher method missing: $name" }
}
if ($ValidateOnly) {
    Write-Host 'PASS: launcher interface, DOOM.WAD and existing graphics profile are available. No game started.'
    exit 0
}

$engine = $data.GetMethod('Engine', $flags).Invoke($null, $null)
$invoke = New-Object object[] 2
$invoke[0] = $Iwad
$invoke[1] = [string[]]@()
$arguments = $data.GetMethod('BundledArguments', $flags).Invoke($null, $invoke)
$arguments += ' -graphics "' + $graphics + '" -warp 1 8 -profilespikes -profilegamesteps'
$out = Join-Path $root ('build\lag-capture\' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-UTC')
New-Item -ItemType Directory -Path $out -Force | Out-Null
Copy-Item -LiteralPath $graphics -Destination (Join-Path $out 'graphics.cfg')
$info = @(
    "Build: $($assembly.GetName().Version)", "UTC: $([DateTime]::UtcNow.ToString('o'))",
    "IWAD: $Iwad", "IWAD SHA256: $((Get-FileHash -LiteralPath $Iwad).Hash)",
    "Engine: $engine", "Engine SHA256: $((Get-FileHash -LiteralPath $engine).Hash)",
    "Arguments: $arguments", 'Profiling adds diagnostic overhead; no gameplay optimization is applied.'
)
$info | Set-Content -LiteralPath (Join-Path $out 'run-info.txt') -Encoding UTF8
Write-Host 'E1M8 tracing: play normally or load your E1M8 save with F3.'
Write-Host 'Reproduce the lowering lag, keep watching for about 10 seconds, then quit normally.'
Write-Host "Capturing to: $out"

# Scope environment overrides to this launch and restore the caller afterward.
$overrides = @{
    HELLFORGE_MAP_CACHE = (Join-Path $launcherHome 'Cache\Maps')
    HELLFORGE_CRASH_DIR = (Join-Path $launcherHome 'crashes')
    HELLFORGE_SCREENSHOTS = (Join-Path $root 'dist\Screenshots')
}
$previous = @{}
try {
    foreach ($key in $overrides.Keys) {
        $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $overrides[$key], 'Process')
    }
    $game = Start-Process -FilePath $engine -ArgumentList $arguments -WorkingDirectory $launcherHome -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput (Join-Path $out 'gameplay.log') -RedirectStandardError (Join-Path $out 'renderer.log')
    "Exit code: $($game.ExitCode)" | Add-Content -LiteralPath (Join-Path $out 'run-info.txt') -Encoding UTF8
} finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
}
Compress-Archive -Path (Join-Path $out '*') -DestinationPath ($out + '.zip')
Write-Host "Send this diagnostic ZIP: $out.zip"

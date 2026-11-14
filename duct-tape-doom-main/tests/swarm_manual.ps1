param(
    [string]$Iwad = 'F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/DOOM2.WAD',
    [string]$Wad = 'F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/dmnsns2k-monsters.wad'
)
$ErrorActionPreference = 'Stop'
$engine = Join-Path $PSScriptRoot 'doom.exe'
if (!(Test-Path -LiteralPath $engine)) { throw 'Run the packaged Test-MAP01.ps1 beside the experimental doom.exe.' }
foreach ($resource in @($Iwad, $Wad)) {
    if (!(Test-Path -LiteralPath $resource)) { throw "Missing WAD: $resource" }
}
$profile = Join-Path $PSScriptRoot 'ManualProfile'
New-Item -ItemType Directory -Force -Path $profile | Out-Null
$graphics = Join-Path $profile 'graphics.cfg'
if (!(Test-Path -LiteralPath $graphics)) {
    @('monster_hibernation 1', 'swarm_debug 1', 'fullscreen 0') | Set-Content -LiteralPath $graphics -Encoding ascii
}
$arguments = @('-iwad', $Iwad, '-file', $Wad, '-warp', '1', '-skill', '4',
    '-nodigging', '-graphics', $graphics, '-config', (Join-Path $profile 'doom.cfg'))
Push-Location $PSScriptRoot
try { & $engine @arguments } finally { Pop-Location }

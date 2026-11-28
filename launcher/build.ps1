param([switch]$LauncherOnly, [string]$OutputPath, [string]$EnginePath, [switch]$Experimental, [switch]$ForceRebuild, [switch]$MonsterProfiling)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($LauncherOnly -and $ForceRebuild) { throw '-ForceRebuild cannot be combined with -LauncherOnly.' }
if ($MonsterProfiling -and ($LauncherOnly -or $EnginePath)) {
    throw '-MonsterProfiling builds its own engine. Use a full build without -EnginePath.'
}
. (Join-Path $root 'tools\Build-Common.ps1')
$nativeBuild = if ($MonsterProfiling) { "$root\build\monster-profiling" } else { "$root\build" }
$profiling = if ($MonsterProfiling) { 'ON' } else { 'OFF' }
if (-not $LauncherOnly) {
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio 2022 C++ Build Tools are required.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$configureArguments = Get-HellforgeConfigureArguments "$root\duct-tape-doom-main" $nativeBuild
& $cmake @configureArguments -G 'Visual Studio 17 2022' -A x64 -DDOOM_BUILD_SDL=ON -DDOOM_RENDER3D=ON -DDOOM_SOUNDFONT=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DBUILD_TESTING=ON -DDOOM_GAME_DATA= -DDOOM_TEST_GPU=OFF -DDOOM_MONSTER_PROFILING=ON$profiling -DDOOM_MONSTER_PROFILE_ARGUMENTS=OFF -DDOOM_MONSTER_BENCHMARK=OFF
if ($LASTEXITCODE) { throw 'Configure failed' }
# Source archives can preserve older timestamps. Always refresh the F4 source
# timestamp so incremental native builds compile its current contents.
(Get-Item -LiteralPath "$root\duct-tape-doom-main\platform\settings_panel.cpp").LastWriteTimeUtc = [DateTime]::UtcNow
$nativeBuildArguments = Get-HellforgeNativeBuildArguments $nativeBuild -ForceRebuild:$ForceRebuild
if ($MonsterProfiling) { $nativeBuildArguments += @('--target', 'doom') }
if ($ForceRebuild) { Write-Host 'Cleaning native build outputs before recompiling all code (ignoring source timestamps).' }
& $cmake @nativeBuildArguments
if ($LASTEXITCODE) { throw 'Engine build failed' }
}
if (-not $OutputPath) {
    $OutputPath = if ($MonsterProfiling) { "$nativeBuild\launcher\DOOM-Hellforge-Monster-Profiling.exe" } elseif ($Experimental) { "$root\build\experimental-release\DOOM-Hellforge-Experimental.exe" } else { "$root\dist\DOOM-Hellforge.exe" }
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Force (Split-Path $OutputPath -Parent) | Out-Null
New-Item -ItemType Directory -Force "$root\build" | Out-Null
if (-not $EnginePath) { $EnginePath = "$nativeBuild\Release\doom.exe" }
$EnginePath = (Resolve-Path -LiteralPath $EnginePath).Path
$version = (Get-Content "$PSScriptRoot\VERSION" -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'launcher/VERSION must contain a four-part numeric version.' }
$engineHash = (Get-FileHash -LiteralPath $EnginePath -Algorithm SHA256).Hash.Substring(0, 12).ToLowerInvariant()
$buildId = [DateTime]::UtcNow.ToString('yyyyMMddHHmmss')
$versionSource = if ($Experimental) { Join-Path (Split-Path $OutputPath -Parent) 'ExperimentalVersion.cs' } else { "$nativeBuild\LauncherVersion.cs" }
$flavor = if ($Experimental) { ' | Experimental' } else { '' }
if ($MonsterProfiling) { $flavor += ' | Monster Profiling' }
@"
using System.Reflection;
[assembly: AssemblyVersion("$version")]
[assembly: AssemblyFileVersion("$version")]
[assembly: AssemblyInformationalVersion("$version | $buildId UTC | engine $engineHash$flavor")]
"@ | Set-Content -LiteralPath $versionSource -Encoding UTF8
$graphicsResource = "$PSScriptRoot\graphics.cfg"
if ($Experimental) {
    $graphicsResource = Join-Path (Split-Path $OutputPath -Parent) 'ExperimentalGraphics.cfg'
    $lines = @(Get-Content -LiteralPath "$PSScriptRoot\graphics.cfg" | Where-Object { $_ -notmatch '^\s*(monster_hibernation|swarm_debug|sprite_frustum_filter)\s+' })
    $lines += @('monster_hibernation 1', 'swarm_debug 0', 'sprite_frustum_filter 1')
    $lines | Set-Content -LiteralPath $graphicsResource -Encoding UTF8
}
# Regenerate even for launcher-only builds; the generator also reads weak-point
# declarations, so checking only settings_panel.cpp's hash is insufficient.
$schemaPath = if ($MonsterProfiling) { "$nativeBuild\settings-schema.xml" } else { "$PSScriptRoot\settings-schema.xml" }
$schemaArgs = @("$root\tools\Generate-LauncherSettings.py", '--output', $schemaPath)
$python = Get-Command python -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($python) {
    & $python.Source @schemaArgs
} else {
    $pythonLauncher = Get-Command py -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $pythonLauncher) { throw 'Python 3.9+ is required to regenerate launcher settings. Install Python and rebuild.' }
    & $pythonLauncher.Source -3 @schemaArgs
}
if ($LASTEXITCODE) { throw 'Launcher settings generation failed. Build stopped to avoid embedding stale settings.' }
$catalogue = [xml](Get-Content -LiteralPath $schemaPath -Raw)
$nativeSettingsHash = (Get-FileHash -LiteralPath "$root\duct-tape-doom-main\platform\settings_panel.cpp" -Algorithm SHA256).Hash
if ($nativeSettingsHash -ne $catalogue.settings.source_sha256) {
    throw 'Generated launcher settings do not match settings_panel.cpp. Build stopped.'
}
$compilerArgs = @("/win32icon:$PSScriptRoot\hellforge.ico", "/resource:$PSScriptRoot\hellforge.ico,hellforge.ico", "/resource:$schemaPath,settings-schema.xml", '/nologo','/codepage:65001','/target:winexe','/platform:x64','/optimize+',"/out:$OutputPath",
    "/resource:$EnginePath,doom.exe", "/resource:$PSScriptRoot\hero.png,hero.png", "/resource:$graphicsResource,graphics.cfg",
    "/resource:$PSScriptRoot\DiggingMaterials.wad,DiggingMaterials.wad", "/resource:$PSScriptRoot\mods\FluidWeapons.pk3,FluidWeapons.pk3",
    "/resource:$PSScriptRoot\mods\FluidWeapons-SFX.pk3,FluidWeapons-SFX.pk3", "/resource:$PSScriptRoot\mods\FluidMonsters.pk3,FluidMonsters.pk3",
    '/r:System.Windows.Forms.dll','/r:System.Drawing.dll','/r:System.Xml.Linq.dll','/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll',
    "$PSScriptRoot\Launcher.cs", "$PSScriptRoot\LanMenu.cs", $versionSource)
if ($Experimental) { $compilerArgs += '/define:HELLFORGE_EXPERIMENTAL' }
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" @compilerArgs
if ($LASTEXITCODE) { throw 'Launcher build failed' }
& (Join-Path $root 'tools\Copy-ReleaseLicenses.ps1') -OutputDirectory (Split-Path $OutputPath -Parent)

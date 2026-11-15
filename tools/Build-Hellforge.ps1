param([switch]$LauncherOnly, [switch]$RecompileShaders, [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Build-Common.ps1')
$root = Split-Path $PSScriptRoot -Parent
$transcribing = $false
try {
    if ($LauncherOnly -and $RecompileShaders) { throw 'Use -RecompileShaders without -LauncherOnly so the regenerated shaders are compiled into the engine.' }
    $logDirectory = Join-Path $root 'build\one-click'
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    $log = Join-Path $logDirectory ('build-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-UTC.log')
    Start-Transcript -LiteralPath $log | Out-Null
    $transcribing = $true
    $compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (!(Test-Path -LiteralPath $compiler)) { throw 'Windows .NET Framework C# compiler was not found.' }
    if (!$LauncherOnly) {
        $cmake = Find-HellforgeCMake
        Write-Host "CMake: $cmake"
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ Build Tools are required. See BUILDING.md.' }
        $install = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (!$install) { throw 'Visual Studio C++ tools are missing. See BUILDING.md.' }
        if (!(Test-Path -LiteralPath (Join-Path $install 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'))) {
            throw 'The existing release build requires Visual Studio CMake support. Add it in the Visual Studio Installer.'
        }
    } elseif (!(Test-Path -LiteralPath (Join-Path $root 'build\Release\doom.exe'))) {
        throw 'No existing engine found. Run Build-Hellforge.bat without -LauncherOnly first.'
    }
    foreach ($resource in @('DiggingMaterials.wad','hero.png','graphics.cfg','mods\FluidWeapons.pk3','mods\FluidWeapons-SFX.pk3','mods\FluidMonsters.pk3')) {
        if (!(Test-Path -LiteralPath (Join-Path $root "launcher\$resource"))) { throw "Missing local build resource: launcher\$resource" }
    }
    if ($RecompileShaders) {
        $shaderArguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Recompile-Shaders.ps1'))
        if ($CheckOnly) { $shaderArguments += '-CheckOnly' }
        & powershell.exe @shaderArguments
        if ($LASTEXITCODE) { throw 'Shader step failed. Engine and launcher build skipped.' }
    }
    if ($CheckOnly) { Write-Host 'PASS: build prerequisites found. No compilation performed.' }
    else {
        Write-Host 'Building HELLFORGE. Close the game and launcher before compiling.'
        $arguments = @{}
        if ($LauncherOnly) { $arguments.LauncherOnly = $true }
        & (Join-Path $root 'launcher\build.ps1') @arguments
        if ($LASTEXITCODE) { throw "Build failed (exit $LASTEXITCODE)." }
        Write-Host 'SUCCESS: dist\DOOM-Hellforge.exe'
        Write-Host 'Engine: build\Release\doom.exe; matching symbols: build\Release\doom.pdb'
    }
    Write-Host "Log: $log"
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    if ($log) { Write-Host "Log: $log" }
    exit 1
} finally {
    if ($transcribing) { Stop-Transcript | Out-Null }
}

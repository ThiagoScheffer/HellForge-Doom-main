# Shared discovery/build arguments; no downloads, installs or changes to PATH.
function Get-HellforgeConfigureArguments([string]$SourceDirectory, [string]$BuildDirectory) {
    $arguments = @('-S', $SourceDirectory, '-B', $BuildDirectory)
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $expected = @{
            CMAKE_HOME_DIRECTORY = [IO.Path]::GetFullPath($SourceDirectory).Replace('\', '/').TrimEnd('/')
            CMAKE_CACHEFILE_DIR = [IO.Path]::GetFullPath($BuildDirectory).Replace('\', '/').TrimEnd('/')
        }
        foreach ($line in Get-Content -LiteralPath $cache) {
            if ($line -match '^(CMAKE_HOME_DIRECTORY|CMAKE_CACHEFILE_DIR):INTERNAL=(.*)$') {
                if ($matches[2].Replace('\', '/').TrimEnd('/') -ine $expected[$matches[1]]) {
                    Write-Host 'Checkout path changed: backing up stale CMake metadata before configuring.'
                    $suffix = '.moved-' + [guid]::NewGuid().ToString('N')
                    foreach ($name in @('CMakeCache.txt', 'CMakeFiles')) {
                        $metadata = Join-Path ([IO.Path]::GetFullPath($BuildDirectory)) $name
                        if (Test-Path -LiteralPath $metadata) {
                            Rename-Item -LiteralPath $metadata -NewName ($name + $suffix) -ErrorAction Stop
                        }
                    }
                    break
                }
            }
        }
    }
    # FetchContent also caches absolute paths in dependency build directories.
    $dependencies = Join-Path $BuildDirectory '_deps'
    if (Test-Path -LiteralPath $dependencies) {
        foreach ($directory in Get-ChildItem -LiteralPath $dependencies -Directory) {
            $dependencyCache = Join-Path $directory.FullName 'CMakeCache.txt'
            if (Test-Path -LiteralPath $dependencyCache) {
                $sourceEntry = Select-String -LiteralPath $dependencyCache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$'
                if ($sourceEntry) {
                    $null = Get-HellforgeConfigureArguments $sourceEntry.Matches[0].Groups[1].Value $directory.FullName
                }
            }
        }
    }
    return $arguments
}

function Get-HellforgeNativeBuildArguments([string]$BuildDirectory, [switch]$ForceRebuild) {
    $arguments = @('--build', $BuildDirectory, '--config', 'Release', '--parallel', '8')
    if ($ForceRebuild) { $arguments += '--clean-first' }
    return $arguments
}

function Find-HellforgeCMake {
    if ($env:HELLFORGE_CMAKE) {
        return (Get-Command $env:HELLFORGE_CMAKE -CommandType Application -ErrorAction Stop).Source
    }
    $command = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) {
            $candidate = Join-Path $install 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }
    throw 'CMake not found. Install Visual Studio C++ Build Tools with CMake support, or set HELLFORGE_CMAKE.'
}

function Find-HellforgeShaderTool([string]$Name, [string]$ExplicitPath, [string]$EnvironmentValue) {
    foreach ($override in @($ExplicitPath, $EnvironmentValue)) {
        if ($override) { return (Get-Command $override -CommandType Application -ErrorAction Stop).Source }
    }
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $candidates = @(Join-Path $PSScriptRoot "shader-tools\$Name.exe")
    if ($env:VULKAN_SDK) { $candidates += Join-Path $env:VULKAN_SDK "Bin\$Name.exe" }
    foreach ($sdk in @(Get-ChildItem 'C:\VulkanSDK' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
        $candidates += Join-Path $sdk.FullName "Bin\$Name.exe"
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "$Name.exe not found. Put it on PATH or in tools\shader-tools\. See BUILDING.md for explicit tool paths."
}

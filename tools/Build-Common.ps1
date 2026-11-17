# Shared discovery/build arguments; no downloads, installs or changes to PATH.
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

# Focused CMake timestamp regression; no engine/launcher build or external modules.
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $root 'tools\Build-Common.ps1')
$cmake = Find-HellforgeCMake
$ninjaCommand = Get-Command ninja -CommandType Application -ErrorAction SilentlyContinue
$ninja = if ($ninjaCommand) { $ninjaCommand.Source } else {
    Join-Path (Split-Path (Split-Path (Split-Path $cmake -Parent) -Parent) -Parent) 'Ninja\ninja.exe'
}
if (!(Test-Path -LiteralPath $ninja -PathType Leaf)) { throw 'This focused fixture requires Ninja (included with Visual Studio CMake).' }
$fixture = Join-Path $root 'build\force-rebuild-check'
$source = Join-Path $fixture 'source'
$output = Join-Path $fixture 'output'
New-Item -ItemType Directory -Force $source | Out-Null
@'
cmake_minimum_required(VERSION 3.21)
project(HellforgeTimestampCheck LANGUAGES NONE)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compiled.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/source.txt" "${CMAKE_CURRENT_BINARY_DIR}/compiled.txt"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source.txt")
add_custom_target(timestamp-check ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/compiled.txt")
'@ | Set-Content -LiteralPath (Join-Path $source 'CMakeLists.txt') -Encoding ASCII
& $cmake -S $source -B $output -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE) { throw 'Fixture configuration failed.' }
$inputFile = Join-Path $source 'source.txt'
$artifact = Join-Path $output 'compiled.txt'
'before edit' | Set-Content -LiteralPath $inputFile -Encoding ASCII
$arguments = Get-HellforgeNativeBuildArguments $output -ForceRebuild
& $cmake @arguments
if ($LASTEXITCODE) { throw 'Initial fixture build failed.' }
'after edit' | Set-Content -LiteralPath $inputFile -Encoding ASCII
(Get-Item -LiteralPath $inputFile).LastWriteTimeUtc = (Get-Item -LiteralPath $artifact).LastWriteTimeUtc.AddDays(-1)
$arguments = Get-HellforgeNativeBuildArguments $output
if ($arguments -contains '--clean-first') { throw 'Ordinary builds must remain incremental.' }
& $cmake @arguments
if ($LASTEXITCODE -or (Get-Content -LiteralPath $artifact -Raw).Trim() -ne 'before edit') { throw 'Fixture did not reproduce the stale-timestamp bug.' }
Write-Host 'PASS: normal incremental build reproduces the stale artifact with older edited source.'
$arguments = Get-HellforgeNativeBuildArguments $output -ForceRebuild
& $cmake @arguments
if ($LASTEXITCODE -or (Get-Content -LiteralPath $artifact -Raw).Trim() -ne 'after edit') { throw 'Forced rebuild did not pick up the edited source.' }
Write-Host 'PASS: V2 native build arguments clean and regenerate the artifact despite its older source timestamp.'

$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $root 'tools/Build-Common.ps1')
$cmake = Find-HellforgeCMake
$fixture = Join-Path $env:TEMP ('hellforge-moved-' + [guid]::NewGuid().ToString('N'))
$original = Join-Path $fixture 'original'
$moved = Join-Path $fixture 'moved'
New-Item -ItemType Directory -Path $original -Force | Out-Null
'cmake_minimum_required(VERSION 3.24)
project(MovedCheckout LANGUAGES NONE)' | Set-Content (Join-Path $original 'CMakeLists.txt')
& $cmake -S $original -B "$original/build"
if ($LASTEXITCODE) { throw 'Initial fixture configure failed.' }
& $cmake -S $original -B "$original/build/_deps/example-subbuild"
if ($LASTEXITCODE) { throw 'Dependency fixture configure failed.' }
Copy-Item -LiteralPath $original -Destination $moved -Recurse
$ErrorActionPreference = 'Continue'
& $cmake -S $moved -B "$moved/build" 2>&1 | Out-Host
$ErrorActionPreference = 'Stop'
if (!$LASTEXITCODE) { throw 'Moved cache did not reproduce the configure error.' }
$arguments = Get-HellforgeConfigureArguments $moved "$moved/build"
if (Test-Path "$moved/build/CMakeCache.txt") { throw 'Moved checkout must back up the stale cache.' }
if (!(Get-ChildItem "$moved/build/CMakeCache.txt.moved-*")) { throw 'Original cache must be preserved.' }
if (Test-Path "$moved/build/_deps/example-subbuild/CMakeCache.txt") { throw 'Moved dependency cache must also be backed up.' }
& $cmake @arguments
if ($LASTEXITCODE) { throw 'Moved checkout recovery failed.' }
$arguments = Get-HellforgeConfigureArguments $moved "$moved/build"
if (!(Test-Path "$moved/build/CMakeCache.txt")) { throw 'Unchanged checkout must preserve its cache.' }
Write-Host 'PASS: moved cache reproduced, recovered, and unchanged cache preserved.'

param([Parameter(Mandatory = $true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'licenses'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $destination 'LICENSE.TXT') -Force
Get-ChildItem -LiteralPath (Join-Path $root 'licenses') -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
}
# Preserve the newer DERP notice separately from the earlier Duct-Tape notice.
Copy-Item -LiteralPath (Join-Path $root 'duct-tape-doom-main/NOTICE.txt') -Destination (Join-Path $destination 'DERP-NOTICE.txt') -Force

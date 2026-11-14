param(
    [Parameter(Mandatory=$true)][string]$Launcher,
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$Iwad,
    [Parameter(Mandatory=$true)][string[]]$Wads
)
$ErrorActionPreference = 'Stop'
$assembly = [Reflection.Assembly]::LoadFile((Resolve-Path -LiteralPath $Launcher).Path)
$data = $assembly.GetType('Data', $true)
$flags = [Reflection.BindingFlags]'NonPublic,Static'
function Check($condition, $message) {
    if (!$condition) { throw "FAIL $message" }
    Write-Output "PASS $message"
}
Check ($data.GetField('ProductName',$flags).GetRawConstantValue().Contains('Experimental')) 'experimental product label'
foreach ($name in @('Home','Profile')) {
    Check ($data.GetField($name,$flags).GetValue($null).EndsWith('Experimental')) "$name is isolated from release storage"
}
$embedded = $assembly.GetManifestResourceStream('doom.exe')
try {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($sha.ComputeHash($embedded)).Replace('-','') } finally { $sha.Dispose() }
} finally { $embedded.Dispose() }
Check ($hash -eq (Get-FileHash -LiteralPath $Engine -Algorithm SHA256).Hash) 'embedded engine matches tested native executable'
$reader = New-Object IO.StreamReader($assembly.GetManifestResourceStream('graphics.cfg'))
try { $graphics = $reader.ReadToEnd() } finally { $reader.Dispose() }
Check ($graphics -match '(?m)^monster_hibernation 1\s*$') 'new experimental profiles enable idle hibernation'
Check ($graphics -match '(?m)^sprite_frustum_filter 1\s*$') 'new experimental profiles enable conservative sprite filtering'
$geometry = $data.GetMethod('GeometryMode',$flags)
Check ($geometry.Invoke($null,@($false)) -eq '-solidmaps') 'default geometry supports wall destruction'
Check ($geometry.Invoke($null,@($true)) -eq '-nodigging') 'classic geometry requires explicit opt-in'
$sight = $data.GetMethod('AmbushSightMode',$flags)
Check ($sight.Invoke($null,@($false)) -eq '') 'ambush sight experiment defaults off'
Check ($sight.Invoke($null,@($true)) -eq ' -ambushsightreuse') 'explicit ambush sight opt-in reaches engine'
foreach ($wad in $Wads) {
    $invokeArgs = New-Object object[] 2
    $invokeArgs[0] = (Resolve-Path -LiteralPath $Iwad).Path
    $invokeArgs[1] = [string[]]@((Resolve-Path -LiteralPath $wad).Path)
    $arguments = $data.GetMethod('Arguments',$flags).Invoke($null,$invokeArgs)
    Check ($arguments.Contains('-solidmaps') -and !$arguments.Contains('-nodigging')) 'normal launch retains destruction backend'
    $quoteArgs = New-Object object[] 1
    $quoteArgs[0] = $invokeArgs[1][0]
    $quoted = $data.GetMethod('Quote',$flags).Invoke($null,$quoteArgs)
    Check ($arguments.Contains('-file '+$quoted)) ('selected WAD routed to engine: '+[IO.Path]::GetFileName($wad))
}

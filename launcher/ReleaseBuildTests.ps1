param(
    [Parameter(Mandatory=$true)][string]$Launcher,
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$Iwad,
    [Parameter(Mandatory=$true)][string]$Version
)
$ErrorActionPreference='Stop'
$script:checks=0
function Check($ok,$name){if(!$ok){throw "FAIL $name"};$script:checks++;Write-Output "PASS $name"}
$launcherPath=(Resolve-Path -LiteralPath $Launcher).Path
$enginePath=(Resolve-Path -LiteralPath $Engine).Path
$a=[Reflection.Assembly]::LoadFile($launcherPath)
$d=$a.GetType('Data',$true);$flags=[Reflection.BindingFlags]'NonPublic,Static'
Check ($d.GetField('ProductName',$flags).GetRawConstantValue() -eq 'DOOM: HELLFORGE') 'normal product identity'
Check ($a.GetName().Version.ToString() -eq $Version) 'launcher four-part release version'
$native=[Diagnostics.FileVersionInfo]::GetVersionInfo($enginePath)
Check ($native.FileVersion -eq $Version) 'matching native release version'
Check ($native.ProductName -eq 'DOOM: HELLFORGE' -and $native.FileDescription -eq 'DOOM: HELLFORGE Engine') 'native product metadata'
Check ($d.GetField('Home',$flags).GetValue($null).EndsWith('DOOM-HellforgeLauncher')) 'normal launcher storage identity'
Check ($d.GetField('Profile',$flags).GetValue($null).EndsWith('DOOM Hellforge')) 'normal game profile identity'
$stream=$a.GetManifestResourceStream('doom.exe');$sha=[Security.Cryptography.SHA256]::Create()
try{$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}
Check ($hash -eq (Get-FileHash -LiteralPath $enginePath).Hash) 'embedded engine equals verified native build'
$label=$d.GetProperty('BuildVersion',$flags).GetValue($null,$null)
Check ($label.Contains($Version) -and $label.Contains($hash.Substring(0,12).ToLowerInvariant()) -and !$label.Contains('Experimental')) 'normal build label and embedded engine hash'
$invoke=New-Object object[] 2;$invoke[0]=(Resolve-Path -LiteralPath $Iwad).Path;$invoke[1]=[string[]]@()
$engineArguments=$d.GetMethod('Arguments',$flags).Invoke($null,$invoke)
Check ($engineArguments.Contains('-solidmaps') -and !$engineArguments.Contains('-nodigging')) 'destruction backend enabled by normal launcher'
Check (!$engineArguments.Contains('-ambushsightreuse')) 'saved F4 preference controls ambush reuse by default'
$reader=New-Object IO.StreamReader($a.GetManifestResourceStream('graphics.cfg'))
try{$cfg=$reader.ReadToEnd()}finally{$reader.Dispose()}
foreach($key in @('ambush_sight_reuse','sprite_frustum_filter','monster_hibernation','eye_injuries')){
 Check ($cfg -notmatch "(?m)^\s*$key\s+1\s*$") "new profile does not force $key on"
}
Write-Output "PASS $script:checks normal-release packaging checks"

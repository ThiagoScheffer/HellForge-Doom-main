# Rebuild the original vector-style HELLFORGE icon. No external tools required.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$frames=@()
foreach($size in @(16,32,48,64,128,256)) {
    $bitmap=New-Object Drawing.Bitmap($size,$size)
    $g=[Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.ScaleTransform($size/256.0,$size/256.0)
    $g.Clear([Drawing.Color]::Transparent)
    $outer=[Drawing.PointF[]]@([Drawing.PointF]::new(128,6),[Drawing.PointF]::new(238,64),[Drawing.PointF]::new(238,194),[Drawing.PointF]::new(128,250),[Drawing.PointF]::new(18,194),[Drawing.PointF]::new(18,64))
    $bg=New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#120E0D'))
    $edge=New-Object Drawing.Pen([Drawing.ColorTranslator]::FromHtml('#FF8A36'),9)
    $g.FillPolygon($bg,$outer);$g.DrawPolygon($edge,$outer)
    $forge=New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#F3E8DE'))
    foreach($rect in @(@(65,84,27,113),@(164,84,27,113),@(92,128,72,25))) {$g.FillRectangle($forge,[int]$rect[0],[int]$rect[1],[int]$rect[2],[int]$rect[3])}
    $flame=[Drawing.PointF[]]@([Drawing.PointF]::new(128,32),[Drawing.PointF]::new(150,72),[Drawing.PointF]::new(145,99),[Drawing.PointF]::new(128,115),[Drawing.PointF]::new(108,99),[Drawing.PointF]::new(105,77),[Drawing.PointF]::new(120,88))
    $fire=New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#FF8A36'));$g.FillPolygon($fire,$flame)
    $stream=New-Object IO.MemoryStream;$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
    $frames+=,@{Size=$size;Bytes=$stream.ToArray()}
    if($size -eq 256){$bitmap.Save((Join-Path $root 'launcher/hellforge-icon.png'),[Drawing.Imaging.ImageFormat]::Png)}
    $stream.Dispose();$g.Dispose();$bitmap.Dispose();$bg.Dispose();$edge.Dispose();$forge.Dispose();$fire.Dispose()
}
$output=[IO.File]::Create((Join-Path $root 'launcher/hellforge.ico'))
$writer=New-Object IO.BinaryWriter($output)
try {
    $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$frames.Count)
    $offset=6+16*$frames.Count
    foreach($frame in $frames){$dimension=if($frame.Size -eq 256){0}else{$frame.Size};$writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([byte]0);$writer.Write([byte]0);$writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$frame.Bytes.Length);$writer.Write([uint32]$offset);$offset+=$frame.Bytes.Length}
    foreach($frame in $frames){$writer.Write([byte[]]$frame.Bytes)}
} finally {$writer.Dispose();$output.Dispose()}
Write-Host 'Generated launcher/hellforge.ico (six sizes) and preview PNG.'

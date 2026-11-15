param([string]$Glslang, [string]$SpirvCross, [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Build-Common.ps1')
$root = Split-Path $PSScriptRoot -Parent
$logDirectory = Join-Path $root 'build\one-click'
$transcribing = $false
try {
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    $log = Join-Path $logDirectory ('shaders-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-UTC.log')
    Start-Transcript -LiteralPath $log | Out-Null
    $transcribing = $true
    $cmake = Find-HellforgeCMake
    $validator = Find-HellforgeShaderTool 'glslangValidator' $Glslang $env:HELLFORGE_GLSLANG
    $cross = Find-HellforgeShaderTool 'spirv-cross' $SpirvCross $env:HELLFORGE_SPIRV_CROSS
    Write-Host "CMake: $cmake"
    Write-Host "GLSL compiler: $validator"
    Write-Host "MSL translator: $cross"
    if ($CheckOnly) { Write-Host 'PASS: shader tools found. No shaders changed.' }
    else {
        # Existing CMake script produces every Vulkan/Metal variant. Publish the
        # header only after the entire compile succeeds, preserving it on error.
        $pending = Join-Path $logDirectory 'gpu_shaders.pending.h'
        $work = Join-Path $root 'build\gpu-shaders'
        & $cmake "-DGLSLANG=$validator" "-DSPIRV_CROSS=$cross" "-DWORK=$work" "-DOUTPUT=$pending" -P (Join-Path $root 'duct-tape-doom-main\scripts\compile_gpu_shaders.cmake')
        if ($LASTEXITCODE) { throw "Shader compiler failed (exit $LASTEXITCODE). Existing header preserved." }
        if (!(Test-Path -LiteralPath $pending -PathType Leaf)) { throw 'Shader compiler produced no header. Existing header preserved.' }
        Move-Item -LiteralPath $pending -Destination (Join-Path $root 'duct-tape-doom-main\platform\gpu_shaders.h') -Force
        Write-Host 'SUCCESS: Vulkan and Metal shader header regenerated.'
        Write-Host 'Run Build-Hellforge.bat next to embed these shaders in the engine and launcher.'
    }
    Write-Host "Log: $log"
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    if ($log) { Write-Host "Log: $log" }
    exit 1
} finally {
    if ($transcribing) { Stop-Transcript | Out-Null }
}

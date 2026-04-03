[CmdletBinding()]
param(
    [string]$ToolchainRoot = $env:AARCH64_TOOLCHAIN_ROOT,
    [string]$RdkafkaRoot = $env:RDKAFKA_ROOT,
    [string]$ShimRoot,
    [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue,
        [switch]$AllowMissing
    )

    if ($AllowMissing) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $PathValue).Path)
}

if (-not $ToolchainRoot) {
    throw "ToolchainRoot is required. Pass -ToolchainRoot or set AARCH64_TOOLCHAIN_ROOT."
}

if (-not $RdkafkaRoot) {
    throw "RdkafkaRoot is required. Pass -RdkafkaRoot or set RDKAFKA_ROOT."
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ToolchainRoot = Resolve-FullPath -PathValue $ToolchainRoot
$RdkafkaRoot = Resolve-FullPath -PathValue $RdkafkaRoot

if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectRoot 'build\arm64-cross-release'
}

if (-not $ShimRoot) {
    $ShimRoot = Join-Path $ProjectRoot 'build\toolchain-shim'
}

$BuildDir = Resolve-FullPath -PathValue $BuildDir -AllowMissing
$ShimRoot = Resolve-FullPath -PathValue $ShimRoot -AllowMissing
$Sysroot = Resolve-FullPath -PathValue (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc')
$Gcc = Resolve-FullPath -PathValue (Join-Path $ToolchainRoot 'bin\gcc.exe')
$Readelf = Join-Path $ToolchainRoot 'bin\readelf.exe'

if (-not (Test-Path -LiteralPath $ShimRoot)) {
    & (Join-Path $PSScriptRoot 'prepare-arm64-toolchain-shim-windows.ps1') `
        -ToolchainRoot $ToolchainRoot `
        -OutputDir $ShimRoot | Out-Null
}

$CommonArgs = @(
    "--sysroot=$Sysroot"
    '-std=c11'
    '-O2'
    '-I', (Join-Path $ProjectRoot 'include')
    '-I', (Join-Path $RdkafkaRoot 'include')
    '-L', (Join-Path $RdkafkaRoot 'lib')
)

$CommonArgs += @('-L', $ShimRoot)

$UsrLib64 = Join-Path $Sysroot 'usr\lib64'
if (Test-Path -LiteralPath $UsrLib64) {
    $CommonArgs += @('-L', $UsrLib64)
}

$CommonArgs += @(
    "-Wl,-rpath-link,$(Join-Path $Sysroot 'lib64')"
    "-Wl,-rpath-link,$UsrLib64"
)

$StaticRdkafka = Join-Path $RdkafkaRoot 'lib\librdkafka.a'
if (-not (Test-Path -LiteralPath $StaticRdkafka)) {
    throw "Could not find $StaticRdkafka"
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

& (Join-Path $PSScriptRoot 'validate-arm64-toolchain-windows.ps1') `
    -ToolchainRoot $ToolchainRoot `
    -AdditionalLibraryDirs @($ShimRoot, $UsrLib64)

$producerArgs = @(
    'src\main.c'
    'src\properties.c'
    'src\trace_config.c'
    'src\kafka_trace_producer.c'
    'src\trace_simulator.c'
    '-o', (Join-Path $BuildDir 'c-kafka-trace-producer')
    $StaticRdkafka
    '-lpthread'
    '-ldl'
    '-lm'
    '-lrt'
)

$consumerArgs = @(
    'src\simple_consumer.c'
    '-o', (Join-Path $BuildDir 'c-kafka-trace-consumer')
    $StaticRdkafka
    '-lpthread'
    '-ldl'
    '-lm'
    '-lrt'
)

$probeArgs = @(
    'src\rdkafka_probe.c'
    '-o', (Join-Path $BuildDir 'c-kafka-rdkafka-probe')
    $StaticRdkafka
    '-lpthread'
    '-ldl'
    '-lm'
    '-lrt'
)

& $Gcc @CommonArgs @producerArgs
if ($LASTEXITCODE -ne 0) {
    throw "Producer build failed."
}

& $Gcc @CommonArgs @consumerArgs
if ($LASTEXITCODE -ne 0) {
    throw "Consumer build failed."
}

& $Gcc @CommonArgs @probeArgs
if ($LASTEXITCODE -ne 0) {
    throw "Probe build failed."
}

Write-Host ""
Write-Host "Direct ARM64 cross build completed:"
Write-Host "  $(Join-Path $BuildDir 'c-kafka-trace-producer')"
Write-Host "  $(Join-Path $BuildDir 'c-kafka-trace-consumer')"
Write-Host "  $(Join-Path $BuildDir 'c-kafka-rdkafka-probe')"

if (Test-Path -LiteralPath $Readelf) {
    Write-Host ""
    Write-Host "Producer architecture:"
    & $Readelf -h (Join-Path $BuildDir 'c-kafka-trace-producer') | Select-String 'Machine|Class'
}

[CmdletBinding()]
param(
    [string]$ToolchainRoot = $env:AARCH64_TOOLCHAIN_ROOT,
    [string]$RdkafkaRoot = $env:RDKAFKA_ROOT,
    [string]$BuildDir,
    [string]$BuildType = 'Release',
    [string]$Generator = 'Ninja',
    [string[]]$AdditionalIncludeDirs = @(),
    [string[]]$AdditionalLibraryDirs = @(),
    [switch]$SkipToolchainProbe
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

function Convert-ToCMakeList {
    param(
        [string[]]$Values
    )

    return (($Values | Where-Object { $_ }) | ForEach-Object { Resolve-FullPath -PathValue $_ }) -join ';'
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot

if (-not $ToolchainRoot) {
    throw "ToolchainRoot is required. Pass -ToolchainRoot or set AARCH64_TOOLCHAIN_ROOT."
}

if (-not $RdkafkaRoot) {
    throw "RdkafkaRoot is required. Pass -RdkafkaRoot or set RDKAFKA_ROOT to an ARM64 librdkafka install prefix."
}

$ToolchainRoot = Resolve-FullPath -PathValue $ToolchainRoot
$RdkafkaRoot = Resolve-FullPath -PathValue $RdkafkaRoot

if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectRoot 'build\arm64-cross-release'
}

$BuildDir = Resolve-FullPath -PathValue $BuildDir -AllowMissing
$ToolchainFile = Resolve-FullPath -PathValue (Join-Path $ProjectRoot 'cmake\toolchains\aarch64-none-linux-gnu.cmake')
$AdditionalIncludeDirs = $AdditionalIncludeDirs | Where-Object { $_ }
$AdditionalLibraryDirs = $AdditionalLibraryDirs | Where-Object { $_ }

if ($AdditionalLibraryDirs.Count -eq 0) {
    $defaultShimDir = Join-Path $ProjectRoot 'build\toolchain-shim'
    & (Join-Path $PSScriptRoot 'prepare-arm64-toolchain-shim-windows.ps1') `
        -ToolchainRoot $ToolchainRoot `
        -OutputDir $defaultShimDir | Out-Null
    $AdditionalLibraryDirs = @($defaultShimDir)
}

if (-not $SkipToolchainProbe) {
    & (Join-Path $PSScriptRoot 'validate-arm64-toolchain-windows.ps1') `
        -ToolchainRoot $ToolchainRoot `
        -AdditionalIncludeDirs $AdditionalIncludeDirs `
        -AdditionalLibraryDirs $AdditionalLibraryDirs
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake.exe was not found in PATH. Install CMake or use scripts\build-arm64-cross-windows-direct.ps1."
}

if ($Generator -eq 'Ninja' -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw "Generator 'Ninja' was requested, but ninja.exe was not found in PATH. Install Ninja or use scripts\build-arm64-cross-windows-direct.ps1."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$configureArgs = @(
    '-S', $ProjectRoot,
    '-B', $BuildDir,
    '-G', $Generator,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile",
    "-DAARCH64_TOOLCHAIN_ROOT=$ToolchainRoot",
    "-DRDKAFKA_ROOT=$RdkafkaRoot"
)

$extraIncludeList = Convert-ToCMakeList -Values $AdditionalIncludeDirs
$extraLibraryList = Convert-ToCMakeList -Values $AdditionalLibraryDirs

if ($extraIncludeList) {
    $configureArgs += "-DAARCH64_EXTRA_INCLUDE_DIRS=$extraIncludeList"
}

if ($extraLibraryList) {
    $configureArgs += "-DAARCH64_EXTRA_LIBRARY_DIRS=$extraLibraryList"
}

& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed."
}

& cmake --build $BuildDir --config $BuildType
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed."
}

Write-Host ""
Write-Host "Cross build completed:"
Write-Host "  $BuildDir\c-kafka-trace-producer"
Write-Host "  $BuildDir\c-kafka-trace-consumer"

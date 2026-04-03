[CmdletBinding()]
param(
    [string]$ToolchainRoot = $env:AARCH64_TOOLCHAIN_ROOT,
    [string]$OutputDir
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

function Find-LibraryMatch {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Roots,
        [Parameter(Mandatory = $true)]
        [string[]]$Patterns
    )

    foreach ($root in $Roots) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }

        foreach ($pattern in $Patterns) {
            $match = Get-ChildItem -Path (Join-Path $root $pattern) -File -ErrorAction SilentlyContinue |
                Sort-Object Name |
                Select-Object -First 1
            if ($match) {
                return $match.FullName
            }
        }
    }

    return $null
}

if (-not $ToolchainRoot) {
    throw "ToolchainRoot is required. Pass -ToolchainRoot or set AARCH64_TOOLCHAIN_ROOT."
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ToolchainRoot = Resolve-FullPath -PathValue $ToolchainRoot

if (-not $OutputDir) {
    $OutputDir = Join-Path $ProjectRoot 'build\toolchain-shim'
}

$OutputDir = Resolve-FullPath -PathValue $OutputDir -AllowMissing
$Sysroot = Resolve-FullPath -PathValue (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc')

$searchRoots = @(
    (Join-Path $Sysroot 'lib64'),
    (Join-Path $Sysroot 'usr\lib64'),
    (Join-Path $Sysroot 'lib'),
    (Join-Path $Sysroot 'usr\lib')
)

$librarySpecs = @(
    @{
        OutputName = 'libpthread.so'
        Patterns = @('libpthread.so', 'libpthread.so.0', 'libpthread-*.so')
    },
    @{
        OutputName = 'libdl.so'
        Patterns = @('libdl.so', 'libdl.so.2', 'libdl-*.so')
    },
    @{
        OutputName = 'libm.so'
        Patterns = @('libm.so', 'libm.so.6', 'libm-*.so')
    },
    @{
        OutputName = 'librt.so'
        Patterns = @('librt.so', 'librt.so.1', 'librt-*.so')
    }
)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

foreach ($spec in $librarySpecs) {
    $sourcePath = Find-LibraryMatch -Roots $searchRoots -Patterns $spec.Patterns
    if (-not $sourcePath) {
        throw "Could not find $($spec.OutputName) under $Sysroot."
    }

    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $OutputDir $spec.OutputName) -Force
}

Write-Host "ARM64 toolchain shim prepared at:"
Write-Host "  $OutputDir"

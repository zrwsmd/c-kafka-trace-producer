[CmdletBinding()]
param(
    [string]$ToolchainRoot = $env:AARCH64_TOOLCHAIN_ROOT,
    [string[]]$AdditionalIncludeDirs = @(),
    [string[]]$AdditionalLibraryDirs = @(),
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Resolve-NormalizedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $PathValue).Path)
}

function Find-ExistingFile {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    return $null
}

function Invoke-NativeProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList,
        [Parameter(Mandatory = $true)]
        [string]$StdoutPath,
        [Parameter(Mandatory = $true)]
        [string]$StderrPath
    )

    $null = Remove-Item -LiteralPath $StdoutPath, $StderrPath -Force -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $FilePath `
        -ArgumentList $ArgumentList `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $StdoutPath `
        -RedirectStandardError $StderrPath

    $output = @()
    if (Test-Path -LiteralPath $StdoutPath) {
        $output += Get-Content -LiteralPath $StdoutPath
    }
    if (Test-Path -LiteralPath $StderrPath) {
        $output += Get-Content -LiteralPath $StderrPath
    }

    return @{
        ExitCode = $process.ExitCode
        Output = $output
    }
}

if (-not $ToolchainRoot) {
    throw "ToolchainRoot is required. Pass -ToolchainRoot or set AARCH64_TOOLCHAIN_ROOT."
}

$ToolchainRoot = Resolve-NormalizedPath -PathValue $ToolchainRoot
$ProjectRoot = Split-Path -Parent $PSScriptRoot

$gcc = Find-ExistingFile @(
    (Join-Path $ToolchainRoot 'bin\gcc.exe'),
    (Join-Path $ToolchainRoot 'bin\gcc')
)
$objdump = Find-ExistingFile @(
    (Join-Path $ToolchainRoot 'bin\objdump.exe'),
    (Join-Path $ToolchainRoot 'bin\objdump')
)
$sysroot = Resolve-NormalizedPath -PathValue (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc')
$probeDir = Join-Path $ProjectRoot 'build\toolchain-c-probe'
$sourcePath = Join-Path $probeDir 'hello.c'
$binaryPath = Join-Path $probeDir 'hello-aarch64'
$stdoutPath = Join-Path $probeDir 'compile.stdout.log'
$stderrPath = Join-Path $probeDir 'compile.stderr.log'
$objdumpPath = Join-Path $probeDir 'objdump.log'

if (-not $gcc) {
    throw "Could not find gcc under $ToolchainRoot\bin."
}

if (-not $objdump) {
    throw "Could not find objdump under $ToolchainRoot\bin."
}

New-Item -ItemType Directory -Force -Path $probeDir | Out-Null
Set-Content -LiteralPath $sourcePath -Encoding Ascii -Value @'
#include <math.h>
#include <pthread.h>
#include <stdio.h>

static void* worker(void* opaque) {
    return opaque;
}

int main(void) {
    pthread_t tid;
    double x = sqrt(16.0);
    pthread_create(&tid, NULL, worker, NULL);
    pthread_join(tid, NULL);
    printf("toolchain-ok %.1f\n", x);
    return 0;
}
'@

$compileArgs = @(
    $sourcePath,
    '-o', $binaryPath,
    "--sysroot=$sysroot",
    '-lpthread',
    '-lm',
    '-ldl',
    '-lrt'
)

foreach ($includeDir in $AdditionalIncludeDirs) {
    if ($includeDir) {
        $compileArgs += @('-isystem', [System.IO.Path]::GetFullPath($includeDir))
    }
}

foreach ($libraryDir in $AdditionalLibraryDirs) {
    if ($libraryDir) {
        $compileArgs += "-L$([System.IO.Path]::GetFullPath($libraryDir))"
    }
}

$compileResult = Invoke-NativeProcess -FilePath $gcc -ArgumentList $compileArgs -StdoutPath $stdoutPath -StderrPath $stderrPath
$compileExitCode = $compileResult.ExitCode
$compileOutput = $compileResult.Output

if ($compileExitCode -ne 0) {
    if (-not $Quiet) {
        $compileOutput | ForEach-Object { Write-Host $_ }
    }

    throw @"
ARM64 toolchain probe failed.

Toolchain root: $ToolchainRoot
Expected sysroot: $sysroot

This project now builds as C rather than C++. If the failure mentions -lpthread, -lm,
or related runtime libraries, add the missing ARM64 library directories via
-AdditionalLibraryDirs and retry.
"@
}

$objdumpResult = Invoke-NativeProcess -FilePath $objdump -ArgumentList @('-f', $binaryPath) -StdoutPath $objdumpPath -StderrPath ($objdumpPath + '.stderr')
$objdumpExitCode = $objdumpResult.ExitCode
$objdumpOutput = $objdumpResult.Output
$objdumpText = ($objdumpOutput -join "`n")

if ($objdumpExitCode -ne 0 -or ($objdumpText -notmatch 'architecture:\s*aarch64')) {
    if (-not $Quiet) {
        $objdumpOutput | ForEach-Object { Write-Host $_ }
    }

    throw "Toolchain probe produced an output binary, but objdump did not identify it as aarch64."
}

if (-not $Quiet) {
    Write-Host "Toolchain probe succeeded."
    Write-Host "  gcc:      $gcc"
    Write-Host "  sysroot:  $sysroot"
    Write-Host "  output:   $binaryPath"
    Write-Host "  arch:     aarch64"
}

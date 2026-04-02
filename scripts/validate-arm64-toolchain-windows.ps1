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

$gxx = Find-ExistingFile @(
    (Join-Path $ToolchainRoot 'bin\g++.exe'),
    (Join-Path $ToolchainRoot 'bin\g++')
)
$objdump = Find-ExistingFile @(
    (Join-Path $ToolchainRoot 'bin\objdump.exe'),
    (Join-Path $ToolchainRoot 'bin\objdump')
)
$sysroot = Resolve-NormalizedPath -PathValue (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc')
$cxxIncludeRoot = Resolve-NormalizedPath -PathValue (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\include\c++\10.3.1')
$cxxTargetIncludeRoot = Resolve-NormalizedPath -PathValue (Join-Path $cxxIncludeRoot 'aarch64-none-linux-gnu')
$cxxBackwardIncludeRoot = Resolve-NormalizedPath -PathValue (Join-Path $cxxIncludeRoot 'backward')

if (-not $gxx) {
    throw "Could not find g++ under $ToolchainRoot\bin."
}

if (-not $objdump) {
    throw "Could not find objdump under $ToolchainRoot\bin."
}

$stdCppCandidates = Get-ChildItem -Path $ToolchainRoot -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^libstdc\+\+(\.a|\.so(\.\d+)*)$' }

$probeDir = Join-Path $ProjectRoot 'build\toolchain-probe'
$sourcePath = Join-Path $probeDir 'hello.cpp'
$binaryPath = Join-Path $probeDir 'hello-aarch64'
$stdoutPath = Join-Path $probeDir 'compile.stdout.log'
$stderrPath = Join-Path $probeDir 'compile.stderr.log'
$objdumpPath = Join-Path $probeDir 'objdump.log'

New-Item -ItemType Directory -Force -Path $probeDir | Out-Null
Set-Content -LiteralPath $sourcePath -Encoding Ascii -Value @'
#include <iostream>

int main() {
    std::cout << "toolchain-ok" << std::endl;
    return 0;
}
'@

$compileArgs = @(
    $sourcePath,
    '-o', $binaryPath,
    "--sysroot=$sysroot",
    '-isystem', $cxxIncludeRoot,
    '-isystem', $cxxTargetIncludeRoot,
    '-isystem', $cxxBackwardIncludeRoot
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

$compileResult = Invoke-NativeProcess -FilePath $gxx -ArgumentList $compileArgs -StdoutPath $stdoutPath -StderrPath $stderrPath
$compileExitCode = $compileResult.ExitCode
$compileOutput = $compileResult.Output

if ($compileExitCode -ne 0) {
    if (-not $stdCppCandidates) {
        Write-Warning "No libstdc++ runtime/library was found under the toolchain root."
        Write-Warning "This toolchain appears to be incomplete for C++ linking. A full SDK is required."
    }

    if (-not $Quiet) {
        $compileOutput | ForEach-Object { Write-Host $_ }
    }

    throw @"
ARM64 toolchain probe failed.

Toolchain root: $ToolchainRoot
Expected sysroot: $sysroot

If the failure mentions bits/c++config.h or libstdc++, the current toolchain does not provide
a complete C++ SDK for this project. Replace it with a full aarch64 Linux SDK, or supplement
the missing include/library directories and pass them via -AdditionalIncludeDirs / -AdditionalLibraryDirs.
"@
}

$objdumpResult = Invoke-NativeProcess -FilePath $objdump -ArgumentList @('-f', $binaryPath) -StdoutPath $objdumpPath -StderrPath ($objdumpPath + '.stderr')
$objdumpExitCode = $objdumpResult.ExitCode
$objdumpOutput = $objdumpResult.Output

if ($objdumpExitCode -ne 0 -or ($objdumpOutput -notmatch 'architecture:\s+aarch64')) {
    if (-not $Quiet) {
        $objdumpOutput | ForEach-Object { Write-Host $_ }
    }

    throw "Toolchain probe produced an output binary, but objdump did not identify it as aarch64."
}

if (-not $Quiet) {
    Write-Host "Toolchain probe succeeded."
    Write-Host "  g++:      $gxx"
    Write-Host "  sysroot:  $sysroot"
    Write-Host "  output:   $binaryPath"
    Write-Host "  arch:     aarch64"
}

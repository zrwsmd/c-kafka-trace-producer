[CmdletBinding()]
param(
    [string]$ToolchainRoot = $env:AARCH64_TOOLCHAIN_ROOT,
    [string]$RdkafkaRoot = $env:RDKAFKA_ROOT,
    [string]$BuildDir,
    [string]$OutputDir,
    [string]$ArchivePath,
    [string[]]$AdditionalLibraryDirs = @()
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

function Should-CopyLibrary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LibraryName
    )

    return $LibraryName -notmatch '^(libc\.so|libm\.so|libpthread\.so|librt\.so|libdl\.so|ld-linux.*\.so|libresolv\.so)'
}

function Get-NeededLibraries {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BinaryPath
    )

    $lines = & $script:Objdump '-p' $BinaryPath 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $BinaryPath"
    }

    $needed = @()
    foreach ($line in $lines) {
        if ($line -match '^\s+NEEDED\s+(.+)$') {
            $needed += $Matches[1].Trim()
        }
    }

    return $needed | Sort-Object -Unique
}

function Resolve-LibraryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LibraryName
    )

    $patterns = New-Object System.Collections.Generic.List[string]
    $patterns.Add($LibraryName)
    if ($LibraryName -match '^(.*?\.so)(\..+)$') {
        $patterns.Add($Matches[1])
        $patterns.Add("$($Matches[1]).*")
    }
    $patterns.Add("$LibraryName*")

    foreach ($dir in $script:SearchDirs) {
        foreach ($pattern in $patterns) {
            $match = Get-ChildItem -Path $dir -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like $pattern } |
                Sort-Object Name |
                Select-Object -First 1
            if ($match) {
                return $match.FullName
            }
        }
    }

    return $null
}

function Copy-LibraryClosure {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathToInspect
    )

    foreach ($neededLibrary in Get-NeededLibraries -BinaryPath $PathToInspect) {
        if (-not (Should-CopyLibrary -LibraryName $neededLibrary)) {
            continue
        }

        if ($script:CopiedLibraries.ContainsKey($neededLibrary)) {
            continue
        }

        $resolvedLibrary = Resolve-LibraryPath -LibraryName $neededLibrary
        if (-not $resolvedLibrary) {
            throw "Could not resolve required runtime library '$neededLibrary'. Search dirs: $($script:SearchDirs -join ', ')"
        }

        $destinationPath = Join-Path $script:LibDir (Split-Path -Leaf $resolvedLibrary)
        Copy-Item -LiteralPath $resolvedLibrary -Destination $destinationPath -Force
        $script:CopiedLibraries[$neededLibrary] = $destinationPath
        Copy-LibraryClosure -PathToInspect $destinationPath
    }
}

$ProjectRoot = Split-Path -Parent $PSScriptRoot

if (-not $ToolchainRoot) {
    throw "ToolchainRoot is required. Pass -ToolchainRoot or set AARCH64_TOOLCHAIN_ROOT."
}

if (-not $RdkafkaRoot) {
    throw "RdkafkaRoot is required. Pass -RdkafkaRoot or set RDKAFKA_ROOT."
}

$ToolchainRoot = Resolve-FullPath -PathValue $ToolchainRoot
$RdkafkaRoot = Resolve-FullPath -PathValue $RdkafkaRoot

if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectRoot 'build\arm64-cross-release'
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $ProjectRoot 'release\arm64'
}
if (-not $ArchivePath) {
    $ArchivePath = Join-Path $ProjectRoot 'c-kafka-trace-producer-arm64.tar.gz'
}

$BuildDir = Resolve-FullPath -PathValue $BuildDir
$OutputDir = Resolve-FullPath -PathValue $OutputDir -AllowMissing
$ArchivePath = Resolve-FullPath -PathValue $ArchivePath -AllowMissing

$ProducerBin = Resolve-FullPath -PathValue (Join-Path $BuildDir 'c-kafka-trace-producer')
$ConsumerBin = Resolve-FullPath -PathValue (Join-Path $BuildDir 'c-kafka-trace-consumer')
$ProbeBin = Resolve-FullPath -PathValue (Join-Path $BuildDir 'c-kafka-rdkafka-probe')

$script:Objdump = Find-ExistingFile @(
    (Join-Path $ToolchainRoot 'bin\objdump.exe'),
    (Join-Path $ToolchainRoot 'bin\objdump')
)

if (-not $script:Objdump) {
    throw "Could not find objdump under $ToolchainRoot\bin."
}

$script:SearchDirs = @(
    (Join-Path $RdkafkaRoot 'lib'),
    (Join-Path $RdkafkaRoot 'lib64'),
    (Join-Path $ToolchainRoot 'lib\gcc\aarch64-none-linux-gnu\10.3.1'),
    (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\lib'),
    (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\lib64'),
    (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc\lib'),
    (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc\lib64'),
    (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc\usr\lib'),
    (Join-Path $ToolchainRoot 'aarch64-none-linux-gnu\libc\usr\lib64')
) + $AdditionalLibraryDirs
$script:SearchDirs = $script:SearchDirs |
    Where-Object { $_ } |
    ForEach-Object { [System.IO.Path]::GetFullPath($_) } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Sort-Object -Unique

if ($script:SearchDirs.Count -eq 0) {
    throw "No runtime library search directories were found."
}

if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}

$script:LibDir = Join-Path $OutputDir 'lib'
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDir 'bin') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDir 'config') | Out-Null
New-Item -ItemType Directory -Force -Path $script:LibDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDir 'deploy') | Out-Null

Copy-Item -LiteralPath $ProducerBin -Destination (Join-Path $OutputDir 'bin\') -Force
Copy-Item -LiteralPath $ConsumerBin -Destination (Join-Path $OutputDir 'bin\') -Force
Copy-Item -LiteralPath $ProbeBin -Destination (Join-Path $OutputDir 'bin\') -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'config\application.properties') -Destination (Join-Path $OutputDir 'config\') -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'deploy\device') -Destination (Join-Path $OutputDir 'deploy\') -Recurse -Force

$script:CopiedLibraries = @{}
Copy-LibraryClosure -PathToInspect $ProducerBin
Copy-LibraryClosure -PathToInspect $ConsumerBin
Copy-LibraryClosure -PathToInspect $ProbeBin

$runtimeReadme = @'
This directory is the runtime package for the ARM64 device.

Contents:
- bin/: native executables
- lib/: runtime shared libraries
- config/: application.properties
- deploy/device/: start/stop/service scripts

Manual start:
  ./deploy/device/start.sh

Validate librdkafka on the target:
  ./bin/c-kafka-rdkafka-probe

Install as service:
  sudo ./deploy/device/install-service.sh /opt/c-kafka-trace-producer
'@
Set-Content -LiteralPath (Join-Path $OutputDir 'README-runtime.txt') -Value $runtimeReadme -Encoding Ascii

$archiveParent = Split-Path -Parent $ArchivePath
New-Item -ItemType Directory -Force -Path $archiveParent | Out-Null
if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}

if (-not (Get-Command tar.exe -ErrorAction SilentlyContinue)) {
    throw "tar.exe was not found in PATH. Windows packaging succeeded, but archive creation cannot continue."
}

$releaseRoot = Split-Path -Parent $OutputDir
$releaseLeaf = Split-Path -Leaf $OutputDir
Push-Location $releaseRoot
try {
    & tar.exe '-czf' $ArchivePath $releaseLeaf
    if ($LASTEXITCODE -ne 0) {
        throw "tar.exe failed to create $ArchivePath"
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Runtime package created at:"
Write-Host "  $OutputDir"
Write-Host "Archive created at:"
Write-Host "  $ArchivePath"

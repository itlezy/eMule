#Requires -Version 7.2
<#
.SYNOPSIS
Builds the standalone eMule unit-test executable and optionally runs it.

.PARAMETER Configuration
The Visual Studio configuration to build.

.PARAMETER Platform
The Visual Studio platform to build.

.PARAMETER Run
Runs the built test executable after a successful build.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',

    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

function Resolve-FirstExisting {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Paths
    )

    foreach ($path in $Paths) {
        if ($path -and (Test-Path -LiteralPath $path)) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    return $null
}

function Get-VsWherePath {
    $cmd = Get-Command 'vswhere.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) {
        return $cmd.Source
    }

    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ }) {
        $candidate = Join-Path $base 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-MSBuildPath {
    $cmd = Get-Command 'MSBuild.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) {
        return $cmd.Source
    }

    $vsWhere = Get-VsWherePath
    if ($vsWhere) {
        $installationPath = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installationPath) {
            $candidate = Join-Path $installationPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ }) {
        $candidate = Resolve-FirstExisting @(
            (Join-Path $base 'Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'),
            (Join-Path $base 'Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe'),
            (Join-Path $base 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'),
            (Join-Path $base 'Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe')
        )
        if ($candidate) {
            return $candidate
        }
    }

    throw 'MSBuild.exe not found.'
}

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')
$projectPath = Join-Path $repoRoot 'srchybrid\emule-tests.vcxproj'
$msbuildPath = Get-MSBuildPath

$arguments = @(
    $projectPath,
    '/m',
    '/nologo',
    '/t:Build',
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform"
)

Write-Output "Building $projectPath ($Platform|$Configuration)"
& $msbuildPath @arguments
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE."
}

if ($Run) {
    $binaryPath = Join-Path $repoRoot ("srchybrid\tests\{0}\{1}\emule-tests.exe" -f $Platform, $Configuration)
    if (-not (Test-Path -LiteralPath $binaryPath)) {
        throw "Built test executable not found: $binaryPath"
    }

    Write-Output "Running $binaryPath"
    & $binaryPath
    if ($LASTEXITCODE -ne 0) {
        throw "The test executable failed with exit code $LASTEXITCODE."
    }
}

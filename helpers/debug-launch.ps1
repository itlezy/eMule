<#
.SYNOPSIS
Builds and launches the repo debug eMule with reproducible diagnostics.

.DESCRIPTION
This helper can clean-reset or patch the LocalAppData eMule config, force a
debug-focused preferences preset, launch the repo debug executable, and bundle
the resulting logs, dumps, and config snapshots into a timestamped artifact
directory under the parent workspace logs folder.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('clean', 'patch', 'none')]
    [string]$PrefsMode = 'clean',

    [Parameter(Mandatory = $false)]
    [int]$TimeoutSec = 0,

    [Parameter(Mandatory = $false)]
    [string]$ExtraArgs = '',

    [Parameter(Mandatory = $false)]
    [string]$ArtifactRoot,

    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-NormalizedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'Path must not be empty.'
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Ensure-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $null = New-Item -ItemType Directory -Force -Path $Path
}

function Copy-DirectorySnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,

        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    if (-not (Test-Path -LiteralPath $SourcePath)) {
        return
    }
    if (Test-Path -LiteralPath $DestinationPath) {
        Remove-Item -LiteralPath $DestinationPath -Recurse -Force
    }
    Ensure-Directory -Path (Split-Path -Parent $DestinationPath)
    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Recurse -Force
}

function Clear-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    Get-ChildItem -LiteralPath $Path -Force | Remove-Item -Recurse -Force
}

function Remove-ResidualDumpFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConfigDir
    )

    if (-not (Test-Path -LiteralPath $ConfigDir)) {
        return 0
    }
    $dumpFiles = @(Get-ChildItem -LiteralPath $ConfigDir -Filter *.dmp -File -ErrorAction SilentlyContinue)
    foreach ($dumpFile in $dumpFiles) {
        Remove-Item -LiteralPath $dumpFile.FullName -Force
    }
    return $dumpFiles.Count
}

function Get-MatchingEmuleProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExePath
    )

    $normalizedExePath = Get-NormalizedPath -Path $ExePath
    $processes = @(Get-Process -Name emule -ErrorAction SilentlyContinue)
    foreach ($process in $processes) {
        try {
            if (-not [string]::IsNullOrWhiteSpace($process.Path) -and
                [string]::Equals((Get-NormalizedPath -Path $process.Path), $normalizedExePath, [System.StringComparison]::OrdinalIgnoreCase)) {
                return $process
            }
        } catch {
        }
    }
    return $null
}

function Invoke-PythonHelper {
    param(
        [Parameter(Mandatory = $true)]
        [string]$HelperPath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & python $HelperPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Python helper failed with exit code $LASTEXITCODE."
    }
}

function Write-Manifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ManifestPath,

        [Parameter(Mandatory = $true)]
        [hashtable]$Data
    )

    $json = $Data | ConvertTo-Json -Depth 8
    Set-Content -LiteralPath $ManifestPath -Value $json -Encoding utf8
}

$scriptDir = Split-Path -Parent $PSCommandPath
$repoRoot = Get-NormalizedPath -Path (Join-Path $scriptDir '..')
$workspaceRoot = Get-NormalizedPath -Path (Join-Path $repoRoot '..')
$buildScriptPath = Join-Path $workspaceRoot '23-build-emule-debug-incremental.cmd'
$exePath = Join-Path $repoRoot 'srchybrid\x64\Debug\emule.exe'
$pythonHelperPath = Join-Path $scriptDir 'debug_launch_helper.py'
$stateRoot = Join-Path $env:LOCALAPPDATA 'eMule'
$configDir = Join-Path $stateRoot 'config'
$logDir = Join-Path $stateRoot 'logs'
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$artifactBase = if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    Join-Path $workspaceRoot 'logs'
} else {
    Get-NormalizedPath -Path $ArtifactRoot
}
$artifactDir = Join-Path $artifactBase "$timestamp-debug-launch"
$manifestPath = Join-Path $artifactDir 'session-manifest.json'
$summaryPath = Join-Path $artifactDir 'session-summary.txt'
$beforeConfigPath = Join-Path $artifactDir 'config-before'
$beforeLogsPath = Join-Path $artifactDir 'logs-before'
$afterConfigPath = Join-Path $artifactDir 'config-after'
$afterLogsPath = Join-Path $artifactDir 'logs-after'
$crtLogSource = Join-Path (Split-Path -Parent $exePath) 'eMule CRT Debug Log.log'
$crtLogTarget = Join-Path $artifactDir 'eMule CRT Debug Log.log'

Ensure-Directory -Path $artifactDir

$manifest = [ordered]@{
    helper = 'debug-launch.ps1'
    started_at = (Get-Date).ToString('o')
    prefs_mode = $PrefsMode
    timeout_sec = $TimeoutSec
    skip_build = [bool]$SkipBuild
    extra_args = $ExtraArgs
    repo_root = $repoRoot
    exe_path = $exePath
    build_script = $buildScriptPath
    state_root = $stateRoot
    config_dir = $configDir
    log_dir = $logDir
    artifact_dir = $artifactDir
    stopped_existing_process = $false
    removed_residual_dumps = 0
    build_status = 'not_run'
    launch_status = 'not_started'
    exit_code = $null
}

Write-Manifest -ManifestPath $manifestPath -Data $manifest

if (-not (Test-Path -LiteralPath $pythonHelperPath -PathType Leaf)) {
    throw "Missing Python helper '$pythonHelperPath'."
}

if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $buildScriptPath -PathType Leaf)) {
        throw "Missing build script '$buildScriptPath'."
    }
    & $buildScriptPath
    if ($LASTEXITCODE -ne 0) {
        $manifest.build_status = 'failed'
        Write-Manifest -ManifestPath $manifestPath -Data $manifest
        throw "Build failed with exit code $LASTEXITCODE."
    }
    $manifest.build_status = 'passed'
    Write-Manifest -ManifestPath $manifestPath -Data $manifest
}

if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Debug executable '$exePath' does not exist."
}

$runningProcess = Get-MatchingEmuleProcess -ExePath $exePath
if ($null -ne $runningProcess) {
    Stop-Process -Id $runningProcess.Id -Force
    $manifest.stopped_existing_process = $true
    $manifest.stopped_process_id = $runningProcess.Id
    Write-Manifest -ManifestPath $manifestPath -Data $manifest
}

Copy-DirectorySnapshot -SourcePath $configDir -DestinationPath $beforeConfigPath
Copy-DirectorySnapshot -SourcePath $logDir -DestinationPath $beforeLogsPath

switch ($PrefsMode) {
    'clean' {
        Ensure-Directory -Path $configDir
        Clear-DirectoryContents -Path $configDir
        Ensure-Directory -Path $logDir
        Clear-DirectoryContents -Path $logDir
        Invoke-PythonHelper -HelperPath $pythonHelperPath -Arguments @(
            'write-debug-prefs',
            '--preferences-ini', (Join-Path $configDir 'preferences.ini')
        )
    }
    'patch' {
        Ensure-Directory -Path $configDir
        Ensure-Directory -Path $logDir
        Clear-DirectoryContents -Path $logDir
        Invoke-PythonHelper -HelperPath $pythonHelperPath -Arguments @(
            'write-debug-prefs',
            '--preferences-ini', (Join-Path $configDir 'preferences.ini'),
            '--preserve-existing'
        )
    }
    'none' {
        Ensure-Directory -Path $configDir
        Ensure-Directory -Path $logDir
    }
}

if ($PrefsMode -ne 'none') {
    $manifest.removed_residual_dumps = Remove-ResidualDumpFiles -ConfigDir $configDir
    Write-Manifest -ManifestPath $manifestPath -Data $manifest
}

$manifest.launch_status = 'starting'
Write-Manifest -ManifestPath $manifestPath -Data $manifest

$startInfo = @{
    FilePath = $exePath
    WorkingDirectory = (Split-Path -Parent $exePath)
    PassThru = $true
}
if (-not [string]::IsNullOrWhiteSpace($ExtraArgs)) {
    $startInfo.ArgumentList = $ExtraArgs
}

$process = Start-Process @startInfo
$manifest.process_id = $process.Id
$manifest.launch_status = 'running'
Write-Manifest -ManifestPath $manifestPath -Data $manifest

if ($TimeoutSec -gt 0) {
    $timedOut = $false
    try {
        Wait-Process -Id $process.Id -Timeout $TimeoutSec -ErrorAction Stop
    } catch {
        $timedOut = $true
    }
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
        Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
        $manifest.launch_status = 'timed_out'
    } else {
        $manifest.launch_status = 'exited'
    }
} else {
    Wait-Process -Id $process.Id
    $manifest.launch_status = 'exited'
}

$completedProcess = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
if ($null -eq $completedProcess) {
    try {
        $manifest.exit_code = $process.ExitCode
    } catch {
        $manifest.exit_code = $null
    }
}

Copy-DirectorySnapshot -SourcePath $configDir -DestinationPath $afterConfigPath
Copy-DirectorySnapshot -SourcePath $logDir -DestinationPath $afterLogsPath
if (Test-Path -LiteralPath $crtLogSource -PathType Leaf) {
    Copy-Item -LiteralPath $crtLogSource -Destination $crtLogTarget -Force
}

$manifest.finished_at = (Get-Date).ToString('o')
Write-Manifest -ManifestPath $manifestPath -Data $manifest

$summary = @(
    "eMule debug launch session"
    "timestamp=$timestamp"
    "prefs_mode=$PrefsMode"
    "build_status=$($manifest.build_status)"
    "launch_status=$($manifest.launch_status)"
    "process_id=$($manifest.process_id)"
    "exit_code=$($manifest.exit_code)"
    "artifact_dir=$artifactDir"
    "config_dir=$configDir"
    "log_dir=$logDir"
)
Set-Content -LiteralPath $summaryPath -Value $summary -Encoding utf8

Write-Host "Artifacts written to: $artifactDir"
Write-Host "Launch status: $($manifest.launch_status)"
if ($manifest.exit_code -ne $null) {
    Write-Host "Exit code: $($manifest.exit_code)"
}

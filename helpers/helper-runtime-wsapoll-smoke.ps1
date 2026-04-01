<#
.SYNOPSIS
Runs a bounded WSAPoll TCP backend smoke session against the repo debug build.

.DESCRIPTION
This helper builds the workspace debug target, clones a disposable `-c`
profile from a seed profile, forces the required bind and disk-logging
preferences, launches `emule.exe` with an explicit config root, samples
process/socket state during the session, and archives logs plus a summary
under the parent workspace `logs` directory.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$ProfileRoot = 'C:\tmp\emule-wsapoll-smoke',

    [Parameter(Mandatory = $false)]
    [string]$SeedProfileRoot = 'C:\tmp\emule-testing',

    [Parameter(Mandatory = $false)]
    [string]$BindInterfaceName = 'hide.me',

    [Parameter(Mandatory = $false)]
    [int]$MonitorSec = 90,

    [Parameter(Mandatory = $false)]
    [int]$PollSec = 10,

    [switch]$SkipBuild,
    [switch]$KeepRunning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-NormalizedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.Path]::GetFullPath($Path)
}

function Ensure-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $null = New-Item -ItemType Directory -Force -Path $Path
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

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,

        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    Ensure-Directory -Path $DestinationPath
    Get-ChildItem -LiteralPath $SourcePath -Force | Copy-Item -Destination $DestinationPath -Recurse -Force
}

function Get-FolderPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolved = Get-NormalizedPath -Path $Path
    if ($resolved.EndsWith('\')) {
        return $resolved
    }
    return $resolved + '\'
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

function Stop-MatchingEmuleProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExePath
    )

    $process = Get-MatchingEmuleProcess -ExePath $ExePath
    if ($null -eq $process) {
        return $null
    }

    try {
        if ($process.CloseMainWindow()) {
            try {
                Wait-Process -Id $process.Id -Timeout 15 -ErrorAction Stop
                return $process.Id
            } catch {
            }
        }
    } catch {
    }

    Stop-Process -Id $process.Id -Force
    return $process.Id
}

function Set-IniValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$IniPath,

        [Parameter(Mandatory = $true)]
        [string]$Section,

        [Parameter(Mandatory = $true)]
        [string]$Key,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Value
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    if (Test-Path -LiteralPath $IniPath) {
        foreach ($line in (Get-Content -LiteralPath $IniPath)) {
            $lines.Add($line)
        }
    }

    $sectionHeader = "[$Section]"
    $sectionIndex = -1
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i] -eq $sectionHeader) {
            $sectionIndex = $i
            break
        }
    }

    if ($sectionIndex -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ne '') {
            $lines.Add('')
        }
        $sectionIndex = $lines.Count
        $lines.Add($sectionHeader)
    }

    $insertIndex = $lines.Count
    for ($i = $sectionIndex + 1; $i -lt $lines.Count; ++$i) {
        if ($lines[$i] -match '^\[.+\]$') {
            $insertIndex = $i
            break
        }
        if ($lines[$i] -match ("^{0}=" -f [regex]::Escape($Key))) {
            $lines[$i] = "$Key=$Value"
            Set-Content -LiteralPath $IniPath -Value $lines -Encoding ascii
            return
        }
    }

    $lines.Insert($insertIndex, "$Key=$Value")
    Set-Content -LiteralPath $IniPath -Value $lines -Encoding ascii
}

function Set-SmokePreferences {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PreferencesPath,

        [Parameter(Mandatory = $true)]
        [string]$IncomingDir,

        [Parameter(Mandatory = $true)]
        [string]$TempDir,

        [Parameter(Mandatory = $true)]
        [string]$BindInterfaceName
    )

    $eMuleValues = [ordered]@{
        AppVersion          = '0.72a.1 x64 DEBUG'
        CreateCrashDump     = '2'
        IncomingDir         = $IncomingDir
        TempDir             = $TempDir
        TempDirs            = ''
        BindInterface       = ''
        BindInterfaceName   = $BindInterfaceName
        BindAddr            = ''
        RandomizePortsOnStartup = '0'
        Reconnect           = '1'
        Autoconnect         = '1'
        NetworkKademlia     = '1'
        NetworkED2K         = '1'
        VerboseOptions      = '1'
        Verbose             = '1'
        FullVerbose         = '1'
        DebugSourceExchange = '1'
        LogBannedClients    = '1'
        LogRatingDescReceived = '1'
        LogSecureIdent      = '1'
        LogFilteredIPs      = '1'
        LogFileSaving       = '1'
        LogA4AF             = '1'
        LogUlDlEvents       = '1'
        DebugServerTCP      = '1'
        DebugServerUDP      = '1'
        DebugServerSources  = '1'
        DebugServerSearches = '1'
        DebugClientTCP      = '1'
        DebugClientUDP      = '1'
        DebugClientKadUDP   = '1'
        SaveLogToDisk       = '1'
        SaveDebugToDisk     = '1'
        CheckDiskspace      = '0'
        DebugHeap           = '1'
    }

    foreach ($entry in $eMuleValues.GetEnumerator()) {
        Set-IniValue -IniPath $PreferencesPath -Section 'eMule' -Key $entry.Key -Value $entry.Value
    }
}

function Get-ProcessSocketsSummary {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId
    )

    return [ordered]@{
        tcp = @(Get-NetTCPConnection -OwningProcess $ProcessId -ErrorAction SilentlyContinue |
            Select-Object State, LocalAddress, LocalPort, RemoteAddress, RemotePort)
        udp = @(Get-NetUDPEndpoint -OwningProcess $ProcessId -ErrorAction SilentlyContinue |
            Select-Object LocalAddress, LocalPort)
    }
}

function Get-ProcessWindowSummary {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process
    )

    return [ordered]@{
        main_window_handle = $Process.MainWindowHandle
        main_window_title = $Process.MainWindowTitle
        responding = $Process.Responding
    }
}

function Test-StartupFailureWindow {
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.IDictionary]$WindowSummary
    )

    $windowTitle = [string]$WindowSummary.main_window_title
    if ([string]::IsNullOrWhiteSpace($windowTitle)) {
        return $false
    }

    return $windowTitle -eq 'Microsoft Visual C++ Runtime Library' -or
        $windowTitle -like '*Assertion*' -or
        $windowTitle -like '*Debug Assertion Failed*'
}

function Get-LogSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogDir
    )

    $snapshot = [ordered]@{}
    foreach ($logFile in @(Get-ChildItem -LiteralPath $LogDir -Filter '*.log' -File -ErrorAction SilentlyContinue)) {
        $snapshot[$logFile.Name] = [ordered]@{
            length = $logFile.Length
            tail   = @(Get-Content -LiteralPath $logFile.FullName -Tail 20 -ErrorAction SilentlyContinue)
        }
    }
    return $snapshot
}

$helperDir = Split-Path -Parent $PSCommandPath
$repoRoot = Get-NormalizedPath -Path (Join-Path $helperDir '..')
$workspaceRoot = Get-NormalizedPath -Path (Join-Path $repoRoot '..')
$buildScriptPath = Join-Path $workspaceRoot '23-build-emule-debug-incremental.cmd'
$exePath = Join-Path $repoRoot 'srchybrid\x64\Debug\emule.exe'
$profileRoot = Get-NormalizedPath -Path $ProfileRoot
$seedProfileRoot = Get-NormalizedPath -Path $SeedProfileRoot
$configDir = Join-Path $profileRoot 'config'
$logDir = Join-Path $profileRoot 'logs'
$incomingDir = Get-FolderPath -Path (Join-Path $profileRoot 'Incoming')
$tempDir = Get-FolderPath -Path (Join-Path $profileRoot 'Temp')
$artifactDir = Join-Path (Join-Path $workspaceRoot 'logs') ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-wsapoll-smoke')
$manifestPath = Join-Path $artifactDir 'session-manifest.json'
$summaryPath = Join-Path $artifactDir 'session-summary.txt'

Ensure-Directory -Path $artifactDir

$bindInterface = Get-NetAdapter -Name $BindInterfaceName -ErrorAction SilentlyContinue
if ($null -eq $bindInterface) {
    throw "Bind interface '$BindInterfaceName' was not found."
}

if (-not (Test-Path -LiteralPath $seedProfileRoot -PathType Container)) {
    throw "Seed profile root '$seedProfileRoot' does not exist."
}

if (-not $SkipBuild) {
    & $buildScriptPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Debug executable '$exePath' does not exist."
}

$stoppedExistingProcessId = Stop-MatchingEmuleProcess -ExePath $exePath

Ensure-Directory -Path $profileRoot
Clear-DirectoryContents -Path $profileRoot
Copy-DirectoryContents -SourcePath $seedProfileRoot -DestinationPath $profileRoot
Ensure-Directory -Path $configDir
Ensure-Directory -Path $logDir
Ensure-Directory -Path $incomingDir
Ensure-Directory -Path $tempDir
Clear-DirectoryContents -Path $logDir

$preferencesPath = Join-Path $configDir 'preferences.ini'
Set-SmokePreferences -PreferencesPath $preferencesPath -IncomingDir $incomingDir -TempDir $tempDir -BindInterfaceName $BindInterfaceName

$manifest = [ordered]@{
    helper = 'helper-runtime-wsapoll-smoke.ps1'
    started_at = (Get-Date).ToString('o')
    exe_path = $exePath
    build_script = $buildScriptPath
    profile_root = $profileRoot
    seed_profile_root = $seedProfileRoot
    bind_interface_name = $BindInterfaceName
    bind_interface_description = $bindInterface.InterfaceDescription
    monitor_sec = $MonitorSec
    poll_sec = $PollSec
    stopped_existing_process_id = $stoppedExistingProcessId
    keep_running = [bool]$KeepRunning
    launch_status = 'starting'
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$startProcessArgs = @{
    FilePath = $exePath
    WorkingDirectory = (Split-Path -Parent $exePath)
    ArgumentList = @('-c', $profileRoot)
    PassThru = $true
}
$process = Start-Process @startProcessArgs

$manifest.process_id = $process.Id
$manifest.launch_status = 'running'
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$samples = New-Object System.Collections.Generic.List[object]
$sawSocketActivity = $false
$startupFailureWindow = $null
$deadline = (Get-Date).AddSeconds($MonitorSec)
while ((Get-Date) -lt $deadline) {
    $runningProcess = Get-MatchingEmuleProcess -ExePath $exePath
    if ($null -eq $runningProcess) {
        break
    }

    $socketSummary = Get-ProcessSocketsSummary -ProcessId $runningProcess.Id
    $windowSummary = Get-ProcessWindowSummary -Process $runningProcess
    if ($socketSummary.tcp.Count -gt 0 -or $socketSummary.udp.Count -gt 0) {
        $sawSocketActivity = $true
    }
    if ($null -eq $startupFailureWindow -and (Test-StartupFailureWindow -WindowSummary $windowSummary)) {
        $startupFailureWindow = $windowSummary.main_window_title
    }

    $samples.Add([pscustomobject][ordered]@{
        timestamp = (Get-Date).ToString('o')
        cpu = $runningProcess.CPU
        working_set = $runningProcess.WorkingSet64
        handles = $runningProcess.Handles
        threads = $runningProcess.Threads.Count
        window = $windowSummary
        sockets = $socketSummary
        logs = Get-LogSnapshot -LogDir $logDir
    })

    Start-Sleep -Seconds $PollSec
}

$finalProcess = Get-MatchingEmuleProcess -ExePath $exePath
$gracefulClose = $false
$forcedStop = $false
if ($null -ne $finalProcess -and -not $KeepRunning) {
    try {
        if ($finalProcess.CloseMainWindow()) {
            try {
                Wait-Process -Id $finalProcess.Id -Timeout 20 -ErrorAction Stop
                $gracefulClose = $true
            } catch {
            }
        }
    } catch {
    }

    $finalProcess = Get-MatchingEmuleProcess -ExePath $exePath
    if ($null -ne $finalProcess) {
        Stop-Process -Id $finalProcess.Id -Force
        $forcedStop = $true
    }
}

$logSnapshot = Get-LogSnapshot -LogDir $logDir
$logSnapshot | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $artifactDir 'log-snapshot.json') -Encoding utf8
$samples | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $artifactDir 'runtime-samples.json') -Encoding utf8
Copy-Item -LiteralPath $preferencesPath -Destination (Join-Path $artifactDir 'preferences.ini') -Force
Copy-Item -LiteralPath (Join-Path $configDir 'server.met') -Destination (Join-Path $artifactDir 'server.met') -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $configDir 'nodes.dat') -Destination (Join-Path $artifactDir 'nodes.dat') -Force -ErrorAction SilentlyContinue

$logFiles = @(Get-ChildItem -LiteralPath $logDir -Filter '*.log' -File -ErrorAction SilentlyContinue)
$hasNonEmptyLog = $false
foreach ($logFile in $logFiles) {
    Copy-Item -LiteralPath $logFile.FullName -Destination (Join-Path $artifactDir $logFile.Name) -Force
    if ($logFile.Length -gt 0) {
        $hasNonEmptyLog = $true
    }
}

$crashDumps = @(Get-ChildItem -LiteralPath $configDir -Filter '*.dmp' -File -ErrorAction SilentlyContinue)
$summary = @(
    "WSAPoll smoke run"
    "exe: $exePath"
    "profile_root: $profileRoot"
    "artifact_dir: $artifactDir"
    "process_id: $($process.Id)"
    "samples: $($samples.Count)"
    "graceful_close: $gracefulClose"
    "forced_stop: $forcedStop"
    "saw_socket_activity: $sawSocketActivity"
    "startup_failure_window: $startupFailureWindow"
    "log_files: $($logFiles.Count)"
    "has_non_empty_log: $hasNonEmptyLog"
    "crash_dumps: $($crashDumps.Count)"
)
Set-Content -LiteralPath $summaryPath -Value $summary -Encoding utf8

if (-not $hasNonEmptyLog) {
    throw "Smoke run completed without any non-empty log files."
}

if ($crashDumps.Count -gt 0) {
    throw "Smoke run produced $($crashDumps.Count) crash dump file(s)."
}

if ($startupFailureWindow) {
    throw "Smoke run detected a startup failure window: $startupFailureWindow"
}

if (-not $sawSocketActivity) {
    throw 'Smoke run completed without observing any TCP or UDP socket activity.'
}

Write-Output "Smoke artifact directory: $artifactDir"

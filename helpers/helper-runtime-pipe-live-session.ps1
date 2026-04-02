<#
.SYNOPSIS
Runs a live eMule session driven through the local pipe API and monitors for hangs.

.DESCRIPTION
This helper launches the repo debug `emule.exe` against an explicit `-c` profile,
forces the required VPN bind and disk logging settings, starts the sibling
`eMule-remote` sidecar, triggers server/Kad connects plus search/download activity
through the pipe-backed HTTP API, samples process and transfer state for a bounded
window, and captures a full dump if the UI stops responding.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$ProfileRoot = 'C:\tmp\emule-testing',

    [Parameter(Mandatory = $false)]
    [string]$BindInterfaceName = 'hide.me',

    [Parameter(Mandatory = $false)]
    [string]$SearchQuery = 'Gran Torino',

    [Parameter(Mandatory = $false)]
    [string[]]$StressQueries = @(),

    [Parameter(Mandatory = $false)]
    [int]$SearchWaitSec = 120,

    [Parameter(Mandatory = $false)]
    [int]$SearchCycleCount = 1,

    [Parameter(Mandatory = $false)]
    [int]$SearchCyclePauseSec = 5,

    [Parameter(Mandatory = $false)]
    [int]$MonitorSec = 480,

    [Parameter(Mandatory = $false)]
    [int]$PollSec = 5,

    [Parameter(Mandatory = $false)]
    [int]$TransferProbeCount = 0,

    [Parameter(Mandatory = $false)]
    [int]$UploadProbeCount = 0,

    [Parameter(Mandatory = $false)]
    [int]$ExtraStatsBurstsPerPoll = 0,

    [Parameter(Mandatory = $false)]
    [int]$RemotePort = 4715,

    [Parameter(Mandatory = $false)]
    [string]$RemoteToken = 'codex-investigation',

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

function Get-MatchingProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProcessName,

        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath
    )

    $normalizedExecutablePath = Get-NormalizedPath -Path $ExecutablePath
    $processes = @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)
    foreach ($process in $processes) {
        try {
            if (-not [string]::IsNullOrWhiteSpace($process.Path) -and
                [string]::Equals((Get-NormalizedPath -Path $process.Path), $normalizedExecutablePath, [System.StringComparison]::OrdinalIgnoreCase)) {
                return $process
            }
        } catch {
        }
    }

    return $null
}

function Stop-MatchingProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProcessName,

        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath
    )

    $process = Get-MatchingProcess -ProcessName $ProcessName -ExecutablePath $ExecutablePath
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

function Stop-ListeningProcessByPort {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Port
    )

    $stoppedProcessIds = New-Object System.Collections.Generic.List[int]
    $listeningConnections = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)
    $owningProcessIds = @($listeningConnections | Select-Object -ExpandProperty OwningProcess -Unique)
    foreach ($processId in $owningProcessIds) {
        if ($null -eq $processId -or $processId -le 0) {
            continue
        }

        try {
            Stop-Process -Id $processId -Force -ErrorAction Stop
            $stoppedProcessIds.Add([int]$processId)
        } catch {
        }
    }

    return @($stoppedProcessIds)
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

function Set-LiveSessionPreferences {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PreferencesPath,

        [Parameter(Mandatory = $true)]
        [string]$BindInterfaceName
    )

    $eMuleValues = [ordered]@{
        AppVersion            = '0.72a.1 x64 DEBUG'
        CreateCrashDump       = '2'
        BindInterface         = ''
        BindInterfaceName     = $BindInterfaceName
        BindAddr              = ''
        RandomizePortsOnStartup = '0'
        Reconnect             = '1'
        Autoconnect           = '1'
        NetworkKademlia       = '1'
        NetworkED2K           = '1'
        VerboseOptions        = '1'
        Verbose               = '1'
        FullVerbose           = '1'
        DebugSourceExchange   = '1'
        LogBannedClients      = '1'
        LogRatingDescReceived = '1'
        LogSecureIdent        = '1'
        LogFilteredIPs        = '1'
        LogFileSaving         = '1'
        LogA4AF               = '1'
        LogUlDlEvents         = '1'
        DebugServerTCP        = '1'
        DebugServerUDP        = '1'
        DebugServerSources    = '1'
        DebugServerSearches   = '1'
        DebugClientTCP        = '1'
        DebugClientUDP        = '1'
        DebugClientKadUDP     = '1'
        SaveLogToDisk         = '1'
        SaveDebugToDisk       = '1'
        CheckDiskspace        = '0'
        EnablePipeApiServer   = '1'
    }

    foreach ($entry in $eMuleValues.GetEnumerator()) {
        Set-IniValue -IniPath $PreferencesPath -Section 'eMule' -Key $entry.Key -Value $entry.Value
    }

    Set-IniValue -IniPath $PreferencesPath -Section 'Remote' -Key 'EnablePipeApiServer' -Value '1'
}

function Start-RedirectedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$StdOutPath,

        [Parameter(Mandatory = $true)]
        [string]$StdErrPath,

        [Parameter(Mandatory = $false)]
        [hashtable]$Environment = @{}
    )

    $startProcessArgs = @{
        FilePath = $FilePath
        ArgumentList = $Arguments
        WorkingDirectory = $WorkingDirectory
        RedirectStandardOutput = $StdOutPath
        RedirectStandardError = $StdErrPath
        Environment = $Environment
        PassThru = $true
        WindowStyle = 'Hidden'
    }

    $process = Start-Process @startProcessArgs
    if ($null -eq $process) {
        throw "Failed to start '$FilePath'."
    }

    return [pscustomobject]@{
        Process = $process
    }
}

function Stop-RedirectedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Handle
    )

    $process = $Handle.Process
    if ($null -ne $process -and -not $process.HasExited) {
        try {
            $null = $process.Kill($true)
            $null = $process.WaitForExit(10000)
        } catch {
        }
    }

}

function Invoke-RemoteJson {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Method,

        [Parameter(Mandatory = $true)]
        [string]$Uri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $false)]
        [object]$Body = $null,

        [Parameter(Mandatory = $false)]
        [int]$TimeoutSec = 8
    )

    $headers = @{
        Authorization = "Bearer $Token"
    }

    $invokeArgs = @{
        Method = $Method
        Uri = $Uri
        Headers = $headers
        TimeoutSec = $TimeoutSec
        ErrorAction = 'Stop'
    }

    if ($null -ne $Body) {
        $invokeArgs.ContentType = 'application/json'
        $invokeArgs.Body = ($Body | ConvertTo-Json -Depth 8 -Compress)
    }

    return Invoke-RestMethod @invokeArgs
}

function Wait-RemoteHealth {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutSec
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        try {
            $health = Invoke-RestMethod -Method Get -Uri "$BaseUri/health" -TimeoutSec 5 -ErrorAction Stop
            if ($health.ok -and $health.pipeConnected) {
                return $health
            }
        } catch {
        }

        Start-Sleep -Seconds 2
    }

    throw "Remote server did not report a connected pipe within $TimeoutSec seconds."
}

function Build-Ed2kLinkFromResult {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$SearchResult
    )

    if ([string]::IsNullOrWhiteSpace($SearchResult.name) -or
        [string]::IsNullOrWhiteSpace($SearchResult.hash) -or
        [uint64]$SearchResult.size -le 0) {
        return $null
    }

    $escapedName = [System.Uri]::EscapeDataString([string]$SearchResult.name)
    return "ed2k://|file|$escapedName|$([uint64]$SearchResult.size)|$($SearchResult.hash)|/"
}

function Get-FallbackEd2kLink {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConfigDir
    )

    foreach ($fileName in @('downloads.txt', 'downloads.bak')) {
        $candidatePath = Join-Path $ConfigDir $fileName
        if (-not (Test-Path -LiteralPath $candidatePath -PathType Leaf)) {
            continue
        }

        foreach ($line in (Get-Content -LiteralPath $candidatePath)) {
            if ($line -match 'ed2k://\|file\|') {
                $parts = $line -split "`t"
                foreach ($part in $parts) {
                    if ($part -like 'ed2k://|file|*') {
                        return $part
                    }
                }
            }
        }
    }

    return $null
}

function Get-ConfiguredSearchQueries {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PrimaryQuery,

        [Parameter(Mandatory = $false)]
        [string[]]$AdditionalQueries = @()
    )

    $queries = New-Object System.Collections.Generic.List[string]
    foreach ($query in @($PrimaryQuery) + $AdditionalQueries) {
        if (-not [string]::IsNullOrWhiteSpace($query)) {
            $queries.Add($query)
        }
    }

    if ($queries.Count -eq 0) {
        throw 'At least one non-empty search query is required.'
    }

    return @($queries)
}

function Get-SampledTransferHashes {
    param(
        [Parameter(Mandatory = $false)]
        [object]$TransfersResponse,

        [Parameter(Mandatory = $true)]
        [int]$Count
    )

    if ($Count -le 0 -or $null -eq $TransfersResponse) {
        return @()
    }

    $transferRows = @()
    if ($TransfersResponse -is [System.Array]) {
        $transferRows = $TransfersResponse
    } elseif ($TransfersResponse.PSObject.Properties.Name -contains 'transfers') {
        $transferRows = @($TransfersResponse.transfers)
    } else {
        $transferRows = @($TransfersResponse)
    }

    $hashes = New-Object System.Collections.Generic.List[string]
    foreach ($transfer in $transferRows) {
        if ($hashes.Count -ge $Count) {
            break
        }

        $hash = [string]$transfer.hash
        if (-not [string]::IsNullOrWhiteSpace($hash)) {
            $hashes.Add($hash)
        }
    }

    return @($hashes)
}

function Invoke-SearchCycle {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $true)]
        [string]$Query,

        [Parameter(Mandatory = $true)]
        [int]$WaitSec,

        [Parameter(Mandatory = $false)]
        [string]$FallbackLink
    )

    $errors = New-Object System.Collections.Generic.List[string]
    $searchSession = $null
    $searchSnapshot = $null
    $selectedDownloadLink = $null
    $stopResult = $null
    $selectedMethod = $null
    $pollCount = 0

    try {
        foreach ($method in @('global', 'kad')) {
            try {
                $searchSession = Invoke-RemoteJson -Method Post -Uri "$BaseUri/api/v2/search/start" -Token $Token -Body @{
                    query = $Query
                    method = $method
                }
                $selectedMethod = $method
                if ($null -ne $searchSession.search_id) {
                    break
                }
            } catch {
                $errors.Add($_.Exception.Message)
                $searchSession = $null
            }
        }

        if ($null -ne $searchSession -and $null -ne $searchSession.search_id) {
            $searchDeadline = (Get-Date).AddSeconds($WaitSec)
            while ((Get-Date) -lt $searchDeadline) {
                $pollCount += 1
                try {
                    $searchSnapshot = Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/search/results?search_id=$($searchSession.search_id)" -Token $Token
                    if ($null -ne $searchSnapshot.results -and $searchSnapshot.results.Count -gt 0) {
                        foreach ($result in $searchSnapshot.results) {
                            if ($result.knownType -eq 'downloading' -or $result.knownType -eq 'downloaded' -or $result.knownType -eq 'cancelled') {
                                continue
                            }

                            $selectedDownloadLink = Build-Ed2kLinkFromResult -SearchResult $result
                            if (-not [string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
                                break
                            }
                        }

                        if (-not [string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
                            break
                        }
                    }
                } catch {
                    $errors.Add($_.Exception.Message)
                }

                Start-Sleep -Seconds 5
            }
        }
    } finally {
        if ([string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
            $selectedDownloadLink = $FallbackLink
        }

        if ($null -ne $searchSession -and $null -ne $searchSession.search_id) {
            try {
                $stopResult = Invoke-RemoteJson -Method Post -Uri "$BaseUri/api/v2/search/stop" -Token $Token -Body @{
                    search_id = $searchSession.search_id
                }
            } catch {
                $errors.Add($_.Exception.Message)
            }
        }
    }

    return [pscustomobject][ordered]@{
        query = $Query
        method = $selectedMethod
        search_session = $searchSession
        search_snapshot = $searchSnapshot
        selected_download_link = $selectedDownloadLink
        stop_result = $stopResult
        poll_count = $pollCount
        errors = @($errors)
    }
}

function Invoke-StressProbe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $true)]
        [int]$TransferProbeCount,

        [Parameter(Mandatory = $true)]
        [int]$UploadProbeCount,

        [Parameter(Mandatory = $true)]
        [int]$ExtraStatsBurstsPerPoll
    )

    $stats = Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/stats/global" -Token $Token
    $transfers = Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/transfers" -Token $Token
    $recentLog = Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/log?limit=40" -Token $Token
    $sampledTransferHashes = Get-SampledTransferHashes -TransfersResponse $transfers -Count $TransferProbeCount

    $transferDetails = New-Object System.Collections.Generic.List[object]
    $transferSources = New-Object System.Collections.Generic.List[object]
    foreach ($hash in $sampledTransferHashes) {
        $transferDetails.Add((Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/transfers/$hash" -Token $Token))
        $transferSources.Add([pscustomobject][ordered]@{
            hash = $hash
            sources = (Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/transfers/$hash/sources" -Token $Token)
        })
    }

    $uploadSnapshots = New-Object System.Collections.Generic.List[object]
    for ($uploadProbeIndex = 0; $uploadProbeIndex -lt $UploadProbeCount; ++$uploadProbeIndex) {
        $uploadSnapshots.Add([pscustomobject][ordered]@{
            list = (Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/uploads/list" -Token $Token)
            queue = (Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/uploads/queue" -Token $Token)
        })
    }

    $extraStatsBursts = New-Object System.Collections.Generic.List[object]
    for ($burstIndex = 0; $burstIndex -lt $ExtraStatsBurstsPerPoll; ++$burstIndex) {
        $extraStatsBursts.Add([pscustomobject][ordered]@{
            stats = (Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/stats/global" -Token $Token)
            recent_log = (Invoke-RemoteJson -Method Get -Uri "$BaseUri/api/v2/log?limit=20" -Token $Token)
        })
    }

    return [pscustomobject][ordered]@{
        stats = $stats
        transfers = $transfers
        recent_log = $recentLog
        sampled_transfer_hashes = $sampledTransferHashes
        transfer_details = @($transferDetails)
        transfer_sources = @($transferSources)
        upload_snapshots = @($uploadSnapshots)
        extra_stats_bursts = @($extraStatsBursts)
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

function Get-LogSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogDir
    )

    $snapshot = [ordered]@{}
    foreach ($logFile in @(Get-ChildItem -LiteralPath $LogDir -Filter '*.log' -File -ErrorAction SilentlyContinue)) {
        $snapshot[$logFile.Name] = [ordered]@{
            length = $logFile.Length
            tail   = @(Get-Content -LiteralPath $logFile.FullName -Tail 40 -ErrorAction SilentlyContinue)
        }
    }

    return $snapshot
}

function Save-HangDump {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId,

        [Parameter(Mandatory = $true)]
        [string]$DumpPath
    )

    $procdumpPath = (Get-Command procdump -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
    if ([string]::IsNullOrWhiteSpace($procdumpPath)) {
        return $null
    }

    $dumpArgs = @('-accepteula', '-ma', $ProcessId.ToString(), $DumpPath)
    $dumpProcess = Start-Process -FilePath $procdumpPath -ArgumentList $dumpArgs -Wait -PassThru -WindowStyle Hidden
    if ($dumpProcess.ExitCode -eq 0 -and (Test-Path -LiteralPath $DumpPath -PathType Leaf)) {
        return $DumpPath
    }

    return $null
}

$helperDir = Split-Path -Parent $PSCommandPath
$repoRoot = Get-NormalizedPath -Path (Join-Path $helperDir '..')
$workspaceRoot = Get-NormalizedPath -Path (Join-Path $repoRoot '..')
$remoteRoot = Get-NormalizedPath -Path (Join-Path $workspaceRoot '..\eMule-remote')
$buildScriptPath = Join-Path $workspaceRoot '23-build-emule-debug-incremental.cmd'
$exePath = Join-Path $repoRoot 'srchybrid\x64\Debug\emule.exe'
$remoteEntryPoint = Join-Path $remoteRoot 'dist\server\index.js'
$profileRoot = Get-NormalizedPath -Path $ProfileRoot
$configDir = Join-Path $profileRoot 'config'
$logDir = Join-Path $profileRoot 'logs'
$preferencesPath = Join-Path $configDir 'preferences.ini'
$artifactDir = Join-Path (Join-Path $workspaceRoot 'logs') ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-pipe-live-session')
$manifestPath = Join-Path $artifactDir 'session-manifest.json'
$summaryPath = Join-Path $artifactDir 'session-summary.txt'
$remoteStdOutPath = Join-Path $artifactDir 'remote-stdout.log'
$remoteStdErrPath = Join-Path $artifactDir 'remote-stderr.log'
$crtLogPath = Join-Path $logDir 'eMule CRT Debug Log.log'
$baseUri = "http://127.0.0.1:$RemotePort"

Ensure-Directory -Path $artifactDir

$bindInterface = Get-NetAdapter -Name $BindInterfaceName -ErrorAction SilentlyContinue
if ($null -eq $bindInterface) {
    throw "Bind interface '$BindInterfaceName' was not found."
}

if (-not (Test-Path -LiteralPath $profileRoot -PathType Container)) {
    throw "Profile root '$profileRoot' does not exist."
}

if (-not (Test-Path -LiteralPath $preferencesPath -PathType Leaf)) {
    throw "Preferences file '$preferencesPath' does not exist."
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

if (-not (Test-Path -LiteralPath $remoteEntryPoint -PathType Leaf)) {
    throw "Remote entry point '$remoteEntryPoint' does not exist."
}

Set-LiveSessionPreferences -PreferencesPath $preferencesPath -BindInterfaceName $BindInterfaceName

$stoppedEmuleProcessId = Stop-MatchingProcess -ProcessName 'emule' -ExecutablePath $exePath
$stoppedRemoteProcessIds = Stop-ListeningProcessByPort -Port $RemotePort

$manifest = [ordered]@{
    helper = 'helper-runtime-pipe-live-session.ps1'
    started_at = (Get-Date).ToString('o')
    exe_path = $exePath
    remote_entry_point = $remoteEntryPoint
    profile_root = $profileRoot
    bind_interface_name = $BindInterfaceName
    bind_interface_description = $bindInterface.InterfaceDescription
    search_query = $SearchQuery
    stress_queries = @($StressQueries)
    search_wait_sec = $SearchWaitSec
    search_cycle_count = $SearchCycleCount
    search_cycle_pause_sec = $SearchCyclePauseSec
    monitor_sec = $MonitorSec
    poll_sec = $PollSec
    transfer_probe_count = $TransferProbeCount
    upload_probe_count = $UploadProbeCount
    extra_stats_bursts_per_poll = $ExtraStatsBurstsPerPoll
    remote_port = $RemotePort
    stopped_existing_emule_process_id = $stoppedEmuleProcessId
    stopped_existing_remote_process_ids = @($stoppedRemoteProcessIds)
    launch_status = 'starting'
}
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$startProcessArgs = @{
    FilePath = $exePath
    WorkingDirectory = (Split-Path -Parent $exePath)
    ArgumentList = @('-assertfile', '-c', $profileRoot)
    PassThru = $true
}
$emuleProcess = Start-Process @startProcessArgs

$nodePath = (Get-Command node -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
if ([string]::IsNullOrWhiteSpace($nodePath)) {
    throw 'Node.js was not found on PATH.'
}

$remoteHandle = Start-RedirectedProcess -FilePath $nodePath -Arguments @($remoteEntryPoint) -WorkingDirectory $remoteRoot -StdOutPath $remoteStdOutPath -StdErrPath $remoteStdErrPath -Environment @{
    EMULE_REMOTE_HOST = '127.0.0.1'
    EMULE_REMOTE_PORT = $RemotePort.ToString()
    EMULE_REMOTE_TOKEN = $RemoteToken
    EMULE_REMOTE_TIMEOUT_MS = '5000'
    EMULE_REMOTE_RECONNECT_MS = '1000'
}

$manifest.process_id = $emuleProcess.Id
$manifest.remote_process_id = $remoteHandle.Process.Id
$manifest.launch_status = 'running'
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$health = Wait-RemoteHealth -BaseUri $baseUri -TimeoutSec 90
$fallbackLink = Get-FallbackEd2kLink -ConfigDir $configDir
$configuredSearchQueries = Get-ConfiguredSearchQueries -PrimaryQuery $SearchQuery -AdditionalQueries $StressQueries

$serversStatus = $null
$kadStatus = $null
$searchSession = $null
$searchSnapshot = $null
$selectedDownloadLink = $null
$transferAddResult = $null
$dumpPath = $null
$freezeReason = $null
$consecutiveApiFailures = 0
$consecutiveNonResponding = 0
$samples = New-Object System.Collections.Generic.List[object]
$searchCycles = New-Object System.Collections.Generic.List[object]

try {
    try {
        $serversStatus = Invoke-RemoteJson -Method Post -Uri "$baseUri/api/v2/servers/connect" -Token $RemoteToken -Body @{}
    } catch {
        $serversStatus = [pscustomobject]@{ error = $_.Exception.Message }
    }

    try {
        $kadStatus = Invoke-RemoteJson -Method Post -Uri "$baseUri/api/v2/kad/connect" -Token $RemoteToken -Body @{}
    } catch {
        $kadStatus = [pscustomobject]@{ error = $_.Exception.Message }
    }

    for ($searchCycleIndex = 0; $searchCycleIndex -lt $SearchCycleCount; ++$searchCycleIndex) {
        $query = $configuredSearchQueries[$searchCycleIndex % $configuredSearchQueries.Count]
        $searchCycle = Invoke-SearchCycle -BaseUri $baseUri -Token $RemoteToken -Query $query -WaitSec $SearchWaitSec -FallbackLink $fallbackLink
        $searchCycles.Add($searchCycle)
        $searchSession = $searchCycle.search_session
        $searchSnapshot = $searchCycle.search_snapshot

        if ([string]::IsNullOrWhiteSpace($selectedDownloadLink) -and -not [string]::IsNullOrWhiteSpace($searchCycle.selected_download_link)) {
            $selectedDownloadLink = $searchCycle.selected_download_link
        }

        if ($searchCycleIndex + 1 -lt $SearchCycleCount -and $SearchCyclePauseSec -gt 0) {
            Start-Sleep -Seconds $SearchCyclePauseSec
        }
    }

    if ([string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
        $selectedDownloadLink = $fallbackLink
    }

    if (-not [string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
        $transferAddResult = Invoke-RemoteJson -Method Post -Uri "$baseUri/api/v2/transfers/add" -Token $RemoteToken -Body @{
            links = @($selectedDownloadLink)
        }
    }

    $deadline = (Get-Date).AddSeconds($MonitorSec)
    while ((Get-Date) -lt $deadline) {
        $runningProcess = Get-MatchingProcess -ProcessName 'emule' -ExecutablePath $exePath
        if ($null -eq $runningProcess) {
            break
        }

        $windowSummary = [ordered]@{
            responding = $runningProcess.Responding
            main_window_title = $runningProcess.MainWindowTitle
            main_window_handle = $runningProcess.MainWindowHandle
        }

        if ($runningProcess.Responding) {
            $consecutiveNonResponding = 0
        } else {
            $consecutiveNonResponding += 1
        }

        $stats = $null
        $transfers = $null
        $recentLog = $null
        $stressProbe = $null
        $apiError = $null
        $apiErrorDetail = $null
        $apiStartedAt = Get-Date
        try {
            $stressProbe = Invoke-StressProbe -BaseUri $baseUri -Token $RemoteToken -TransferProbeCount $TransferProbeCount -UploadProbeCount $UploadProbeCount -ExtraStatsBurstsPerPoll $ExtraStatsBurstsPerPoll
            $stats = $stressProbe.stats
            $transfers = $stressProbe.transfers
            $recentLog = $stressProbe.recent_log
            $consecutiveApiFailures = 0
        } catch {
            $apiError = $_.Exception.Message
            $apiErrorDetail = $_ | Out-String
            $consecutiveApiFailures += 1
        }
        $apiDurationMs = [int](((Get-Date) - $apiStartedAt).TotalMilliseconds)

        $samples.Add([pscustomobject][ordered]@{
            timestamp = (Get-Date).ToString('o')
            cpu = $runningProcess.CPU
            working_set = $runningProcess.WorkingSet64
            handles = $runningProcess.Handles
            threads = $runningProcess.Threads.Count
            window = $windowSummary
            sockets = Get-ProcessSocketsSummary -ProcessId $runningProcess.Id
            api_duration_ms = $apiDurationMs
            api_error = $apiError
            api_error_detail = $apiErrorDetail
            stats = $stats
            transfers = $transfers
            recent_log = $recentLog
            stress_probe = $stressProbe
            disk_logs = Get-LogSnapshot -LogDir $logDir
        })

        if ($consecutiveNonResponding -ge 3) {
            $freezeReason = "process stopped responding for $consecutiveNonResponding consecutive polls"
            break
        }

        if ($consecutiveApiFailures -ge 3) {
            $freezeReason = "pipe-backed API failed for $consecutiveApiFailures consecutive polls"
            break
        }

        Start-Sleep -Seconds $PollSec
    }

    if (-not [string]::IsNullOrWhiteSpace($freezeReason)) {
        $runningProcess = Get-MatchingProcess -ProcessName 'emule' -ExecutablePath $exePath
        if ($null -ne $runningProcess) {
            $dumpPath = Join-Path $artifactDir ("emule-freeze-{0}.dmp" -f $runningProcess.Id)
            $dumpPath = Save-HangDump -ProcessId $runningProcess.Id -DumpPath $dumpPath
        }
    }
} finally {
    if (-not $KeepRunning) {
        $runningProcess = Get-MatchingProcess -ProcessName 'emule' -ExecutablePath $exePath
        if ($null -ne $runningProcess) {
            try {
                if ($runningProcess.CloseMainWindow()) {
                    try {
                        Wait-Process -Id $runningProcess.Id -Timeout 20 -ErrorAction Stop
                    } catch {
                    }
                }
            } catch {
            }

            $runningProcess = Get-MatchingProcess -ProcessName 'emule' -ExecutablePath $exePath
            if ($null -ne $runningProcess) {
                Stop-Process -Id $runningProcess.Id -Force
            }
        }
    }

    Stop-RedirectedProcess -Handle $remoteHandle
}

$finalLogSnapshot = Get-LogSnapshot -LogDir $logDir
$samples | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $artifactDir 'runtime-samples.json') -Encoding utf8
$finalLogSnapshot | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $artifactDir 'log-snapshot.json') -Encoding utf8
Copy-Item -LiteralPath $preferencesPath -Destination (Join-Path $artifactDir 'preferences.ini') -Force
if (Test-Path -LiteralPath $crtLogPath -PathType Leaf) {
    Copy-Item -LiteralPath $crtLogPath -Destination (Join-Path $artifactDir 'eMule CRT Debug Log.log') -Force
}

$searchSessionId = $null
if ($null -ne $searchSession -and $searchSession.PSObject.Properties.Name -contains 'search_id') {
    $searchSessionId = $searchSession.search_id
}

$summary = [ordered]@{
    helper = 'helper-runtime-pipe-live-session.ps1'
    artifact_dir = $artifactDir
    profile_root = $profileRoot
    emule_process_id = $emuleProcess.Id
    remote_process_id = $remoteHandle.Process.Id
    remote_health = $health
    server_connect = $serversStatus
    kad_connect = $kadStatus
    search_session = $searchSession
    search_cycles = @($searchCycles.ToArray())
    selected_download_link = $selectedDownloadLink
    transfer_add_result = $transferAddResult
    freeze_reason = $freezeReason
    dump_path = $dumpPath
    sample_count = $samples.Count
    finished_at = (Get-Date).ToString('o')
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $manifestPath -Encoding utf8
@(
    "Pipe live session"
    "artifact_dir: $artifactDir"
    "profile_root: $profileRoot"
    "search_query: $SearchQuery"
    "search_cycle_count: $SearchCycleCount"
    "search_session: $searchSessionId"
    "selected_download_link: $selectedDownloadLink"
    "freeze_reason: $freezeReason"
    "dump_path: $dumpPath"
    "sample_count: $($samples.Count)"
) | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Output "Pipe live session artifact directory: $artifactDir"

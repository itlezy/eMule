<#
.SYNOPSIS
Runs a live eMule session driven through the local pipe API and monitors for hangs.

.DESCRIPTION
This helper launches the repo debug `emule.exe` against an explicit `-c` profile,
forces the required VPN bind and disk logging settings, starts the sibling
`eMule-remote` sidecar, triggers server/Kad connects plus search/download activity
through the pipe-backed HTTP API, runs deterministic matrix scenarios plus longer
soak churn, samples process and transfer state for a bounded window, and captures
a full dump if the UI stops responding.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$ProfileRoot = 'C:\tmp\emule-testing',

    [Parameter(Mandatory = $false)]
    [string]$SeedRoot = '',

    [Parameter(Mandatory = $false)]
    [string]$SessionManifestPath = '',

    [Parameter(Mandatory = $false)]
    [string]$BindInterfaceName = 'hide.me',

    [Parameter(Mandatory = $false)]
    [string]$SearchQuery = '1080p',

    [Parameter(Mandatory = $false)]
    [string[]]$StressQueries = @(),

    [Parameter(Mandatory = $false)]
    [ValidateSet('balanced', 'matrix', 'soak')]
    [string]$ScenarioProfile = 'balanced',

    [Parameter(Mandatory = $false)]
    [int]$MatrixRepeatCount = 1,

    [switch]$StrictMatrix,

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
    [int]$TransferChurnCycles = 0,

    [Parameter(Mandatory = $false)]
    [int]$TransfersPerChurnCycle = 3,

    [Parameter(Mandatory = $false)]
    [int]$TransferChurnPauseMs = 750,

    [Parameter(Mandatory = $false)]
    [int]$PipeWarmupSec = 12,

    [Parameter(Mandatory = $false)]
    [int]$RemotePort = 4715,

    [Parameter(Mandatory = $false)]
    [string]$RemoteToken = 'codex-investigation',

    [switch]$LaunchOnly,
    [switch]$SkipBuild,
    [switch]$KeepRunning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PipeCommandTranscript = New-Object System.Collections.Generic.List[object]
$script:PipeScenarioResults = New-Object System.Collections.Generic.List[object]
$script:CurrentScenarioContext = $null
$script:CurrentScenarioStep = $null

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

function Publish-DirectorySnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDirectory,

        [Parameter(Mandatory = $true)]
        [string]$DestinationDirectory
    )

    if (Test-Path -LiteralPath $DestinationDirectory -PathType Container) {
        Remove-Item -LiteralPath $DestinationDirectory -Recurse -Force
    }

    Ensure-Directory -Path $DestinationDirectory
    $sourceEntries = @(Get-ChildItem -LiteralPath $SourceDirectory -Force -ErrorAction SilentlyContinue)
    foreach ($sourceEntry in $sourceEntries) {
        Copy-Item -LiteralPath $sourceEntry.FullName -Destination $DestinationDirectory -Recurse -Force
    }
}

function Publish-LatestDirectoryPointer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TargetDirectory,

        [Parameter(Mandatory = $true)]
        [string]$LatestDirectory
    )

    $latestParentDirectory = Split-Path -Parent $LatestDirectory
    if (-not [string]::IsNullOrWhiteSpace($latestParentDirectory)) {
        Ensure-Directory -Path $latestParentDirectory
    }

    if (Test-Path -LiteralPath $LatestDirectory) {
        Remove-Item -LiteralPath $LatestDirectory -Recurse -Force
    }

    <#
    * @brief Keep one stable path that always points at the newest per-run working profile.
    #>
    $null = New-Item -ItemType Junction -Path $LatestDirectory -Target $TargetDirectory -Force
}

function Publish-HarnessExecutableCopy {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceExecutablePath,

        [Parameter(Mandatory = $true)]
        [string]$HarnessExecutablePath
    )

    if (-not (Test-Path -LiteralPath $SourceExecutablePath -PathType Leaf)) {
        throw "Source executable '$SourceExecutablePath' does not exist."
    }

    $harnessParentDirectory = Split-Path -Parent $HarnessExecutablePath
    if (-not [string]::IsNullOrWhiteSpace($harnessParentDirectory)) {
        Ensure-Directory -Path $harnessParentDirectory
    }

    <#
    * @brief Stage a harness-owned binary copy so process listings, dumps, and cleanup clearly identify live harness sessions.
    #>
    Copy-Item -LiteralPath $SourceExecutablePath -Destination $HarnessExecutablePath -Force
}

function Initialize-LiveSessionProfile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SeedRoot,

        [Parameter(Mandatory = $true)]
        [string]$WorkingProfileRoot
    )

    $seedConfigDir = Join-Path $SeedRoot 'config'
    if (-not (Test-Path -LiteralPath $seedConfigDir -PathType Container)) {
        throw "Seed config directory '$seedConfigDir' does not exist."
    }

    $workingConfigDir = Join-Path $WorkingProfileRoot 'config'
    Ensure-Directory -Path $workingConfigDir
    Ensure-Directory -Path (Join-Path $WorkingProfileRoot 'Incoming')
    Ensure-Directory -Path (Join-Path $WorkingProfileRoot 'Temp')
    Ensure-Directory -Path (Join-Path $WorkingProfileRoot 'logs')

    foreach ($seedFileName in @('preferences.ini', 'nodes.dat', 'server.met')) {
        $seedFilePath = Join-Path $seedConfigDir $seedFileName
        if (-not (Test-Path -LiteralPath $seedFilePath -PathType Leaf)) {
            throw "Seed file '$seedFilePath' does not exist."
        }

        Copy-Item -LiteralPath $seedFilePath -Destination (Join-Path $workingConfigDir $seedFileName) -Force
    }
}

function Write-SessionManifest {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Manifest,

        [Parameter(Mandatory = $true)]
        [string[]]$Paths
    )

    $json = $Manifest | ConvertTo-Json -Depth 12
    foreach ($path in $Paths) {
        $parentDirectory = Split-Path -Parent $path
        if (-not [string]::IsNullOrWhiteSpace($parentDirectory)) {
            Ensure-Directory -Path $parentDirectory
        }

        $json | Set-Content -LiteralPath $path -Encoding utf8
    }
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

function Stop-ProcessesByCommandLinePattern {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProcessName,

        [Parameter(Mandatory = $true)]
        [string]$CommandPattern
    )

    $stoppedProcessIds = New-Object System.Collections.Generic.List[int]
    $normalizedPattern = $CommandPattern.ToLowerInvariant()
    foreach ($process in @(Get-CimInstance Win32_Process -Filter ("Name='{0}.exe'" -f $ProcessName) -ErrorAction SilentlyContinue)) {
        $commandLine = [string]$process.CommandLine
        if ([string]::IsNullOrWhiteSpace($commandLine)) {
            continue
        }

        if ($commandLine.ToLowerInvariant().Contains($normalizedPattern)) {
            try {
                Stop-Process -Id ([int]$process.ProcessId) -Force -ErrorAction Stop
                $stoppedProcessIds.Add([int]$process.ProcessId)
            } catch {
            }
        }
    }

    return @($stoppedProcessIds)
}

function Get-ProcessesByCommandLinePattern {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProcessName,

        [Parameter(Mandatory = $true)]
        [string]$CommandPattern
    )

    $matches = @()
    $normalizedPattern = $CommandPattern.ToLowerInvariant()
    foreach ($process in @(Get-CimInstance Win32_Process -Filter ("Name='{0}.exe'" -f $ProcessName) -ErrorAction SilentlyContinue)) {
        $commandLine = [string]$process.CommandLine
        if ([string]::IsNullOrWhiteSpace($commandLine)) {
            continue
        }

        if ($commandLine.ToLowerInvariant().Contains($normalizedPattern)) {
            $matches += [pscustomobject]@{
                process_id = [int]$process.ProcessId
                process_name = $ProcessName
                executable_path = [string]$process.ExecutablePath
                command_line = $commandLine
            }
        }
    }

    return @($matches)
}

function Get-TrackedProcessMatches {
    param(
        [Parameter(Mandatory = $true)]
        [object]$TrackedProcess
    )

    $matchKind = [string]$TrackedProcess.match_kind
    $processName = [string]$TrackedProcess.process_name
    $executablePath = [string]$TrackedProcess.executable_path
    $commandPattern = [string]$TrackedProcess.command_pattern

    if ($matchKind -eq 'path') {
        $matches = @()
        $normalizedExecutablePath = Get-NormalizedPath -Path $executablePath
        foreach ($process in @(Get-Process -Name $processName -ErrorAction SilentlyContinue)) {
            try {
                $processPath = [string]$process.Path
                if (-not [string]::IsNullOrWhiteSpace($processPath) -and
                    ((Get-NormalizedPath -Path $processPath).ToLowerInvariant() -eq $normalizedExecutablePath.ToLowerInvariant())) {
                    $matches += [pscustomobject]@{
                        process_id = [int]$process.Id
                        process_name = $processName
                        executable_path = $processPath
                        command_line = $null
                    }
                }
            } catch {
            }
        }

        return @($matches)
    }

    if ($matchKind -eq 'command_line') {
        return @(Get-ProcessesByCommandLinePattern -ProcessName $processName -CommandPattern $commandPattern)
    }

    throw "Unsupported tracked process match kind '$matchKind'."
}

function Stop-TrackedProcessById {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId
    )

    try {
        $process = Get-Process -Id $ProcessId -ErrorAction Stop
    } catch {
        return $false
    }

    try {
        if ($process.CloseMainWindow()) {
            try {
                Wait-Process -Id $ProcessId -Timeout 10 -ErrorAction Stop
                return $true
            } catch {
            }
        }
    } catch {
    }

    try {
        Stop-Process -Id $ProcessId -Force -ErrorAction Stop
        return $true
    } catch {
    }

    return $false
}

function Invoke-TrackedProcessCleanup {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$TrackedProcesses,

        [Parameter(Mandatory = $false)]
        [int]$AttemptCount = 2,

        [Parameter(Mandatory = $false)]
        [int]$WaitMilliseconds = 1000
    )

    $attempts = 0
    $stoppedProcessIds = @()
    $leftoverProcesses = @()

    for ($attempt = 1; $attempt -le $AttemptCount; ++$attempt) {
        $attempts = $attempt
        foreach ($trackedProcess in $TrackedProcesses) {
            foreach ($match in @(Get-TrackedProcessMatches -TrackedProcess $trackedProcess)) {
                $matchProcessId = [int]$match.process_id
                if ($stoppedProcessIds -contains $matchProcessId) {
                    continue
                }

                if (Stop-TrackedProcessById -ProcessId $matchProcessId) {
                    $stoppedProcessIds += $matchProcessId
                }
            }
        }

        if ($WaitMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $WaitMilliseconds
        }

        $leftoverProcesses = @()
        foreach ($trackedProcess in $TrackedProcesses) {
            foreach ($match in @(Get-TrackedProcessMatches -TrackedProcess $trackedProcess)) {
                $leftoverProcesses += [pscustomobject]@{
                    kind = $trackedProcess.kind
                    process_id = [int]$match.process_id
                    process_name = $match.process_name
                    executable_path = $match.executable_path
                    command_line = $match.command_line
                    match_kind = $trackedProcess.match_kind
                }
            }
        }

        if ($leftoverProcesses.Count -eq 0) {
            break
        }
    }

    return [ordered]@{
        attempts = $attempts
        success = ($leftoverProcesses.Count -eq 0)
        stopped_process_ids = @($stoppedProcessIds)
        leftover_processes = @($leftoverProcesses)
        leftover_process_ids = @($leftoverProcesses | ForEach-Object { [int]$_.process_id } | Select-Object -Unique)
        error = $null
    }
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

function Set-IniValueEverywhere {
    param(
        [Parameter(Mandatory = $true)]
        [string]$IniPath,

        [Parameter(Mandatory = $true)]
        [string]$Key,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Value
    )

    if (-not (Test-Path -LiteralPath $IniPath -PathType Leaf)) {
        return
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($line in (Get-Content -LiteralPath $IniPath)) {
        if ($line -match ("^{0}=" -f [regex]::Escape($Key))) {
            $lines.Add("$Key=$Value")
        } else {
            $lines.Add($line)
        }
    }

    Set-Content -LiteralPath $IniPath -Value $lines -Encoding ascii
}

function Set-LiveSessionPreferences {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PreferencesPath,

        [Parameter(Mandatory = $true)]
        [string]$ProfileRoot,

        [Parameter(Mandatory = $true)]
        [string]$BindInterfaceName
    )

    $incomingDir = (Join-Path $ProfileRoot 'Incoming').TrimEnd('\') + '\'
    $tempDir = (Join-Path $ProfileRoot 'Temp').TrimEnd('\') + '\'
    $eMuleValues = [ordered]@{
        AppVersion            = '0.72a.1 x64 DEBUG'
        IncomingDir           = $incomingDir
        TempDir               = $tempDir
        TempDirs              = ''
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
    foreach ($globalEntry in @(
        @{ Key = 'IncomingDir'; Value = $incomingDir },
        @{ Key = 'TempDir'; Value = $tempDir },
        @{ Key = 'TempDirs'; Value = '' },
        @{ Key = 'BindInterfaceName'; Value = $BindInterfaceName },
        @{ Key = 'EnablePipeApiServer'; Value = '1' }
    )) {
        Set-IniValueEverywhere -IniPath $PreferencesPath -Key $globalEntry.Key -Value $globalEntry.Value
    }
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

    $previousEnvironment = @{}
    foreach ($entry in $Environment.GetEnumerator()) {
        $name = [string]$entry.Key
        $previousEnvironment[$name] = [pscustomobject]@{
            Exists = $null -ne [System.Environment]::GetEnvironmentVariable($name, 'Process')
            Value = [System.Environment]::GetEnvironmentVariable($name, 'Process')
        }
        [System.Environment]::SetEnvironmentVariable($name, [string]$entry.Value, 'Process')
    }

    $startProcessArgs = @{
        FilePath = $FilePath
        ArgumentList = $Arguments
        WorkingDirectory = $WorkingDirectory
        RedirectStandardOutput = $StdOutPath
        RedirectStandardError = $StdErrPath
        PassThru = $true
        WindowStyle = 'Hidden'
    }

    <#*
     * @brief Launch the redirected child after staging its environment in the current PowerShell process.
     *
     * Windows PowerShell does not expose Start-Process -Environment, so the child inherits these
     * temporary process-scoped variables and the caller's environment is restored immediately after.
     #>
    try {
        $process = Start-Process @startProcessArgs
    } finally {
        foreach ($entry in $previousEnvironment.GetEnumerator()) {
            if ($entry.Value.Exists) {
                [System.Environment]::SetEnvironmentVariable($entry.Key, [string]$entry.Value.Value, 'Process')
            } else {
                [System.Environment]::SetEnvironmentVariable($entry.Key, $null, 'Process')
            }
        }
    }
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

function Get-NormalizedApiValue {
    param(
        [Parameter(Mandatory = $false)]
        [AllowNull()]
        [object]$Value,

        [Parameter(Mandatory = $false)]
        [int]$Depth = 0
    )

    if ($Depth -ge 2) {
        if ($null -eq $Value) {
            return $null
        }

        return [string]$Value
    }

    if ($null -eq $Value) {
        return $null
    }

    if ($Value -is [string] -or $Value -is [char] -or $Value -is [bool] -or
        $Value -is [byte] -or $Value -is [int16] -or $Value -is [int32] -or
        $Value -is [int64] -or $Value -is [uint16] -or $Value -is [uint32] -or
        $Value -is [uint64] -or $Value -is [double] -or $Value -is [decimal]) {
        return $Value
    }

    if ($Value -is [System.Collections.IDictionary]) {
        $summary = [ordered]@{}
        foreach ($key in @($Value.Keys | Select-Object -First 8)) {
            $summary[[string]$key] = Get-NormalizedApiValue -Value $Value[$key] -Depth ($Depth + 1)
        }
        return $summary
    }

    if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string])) {
        $items = @($Value)
        $summary = [ordered]@{
            kind = 'array'
            count = $items.Count
        }
        if ($items.Count -gt 0) {
            $summary.first = Get-NormalizedApiValue -Value $items[0] -Depth ($Depth + 1)
        }
        return $summary
    }

    $propertyNames = @($Value.PSObject.Properties.Name)
    if ($propertyNames.Count -gt 0) {
        $summary = [ordered]@{}
        foreach ($propertyName in @($propertyNames | Select-Object -First 8)) {
            $summary[$propertyName] = Get-NormalizedApiValue -Value $Value.$propertyName -Depth ($Depth + 1)
        }
        return $summary
    }

    return [string]$Value
}

function Get-FirstScalarValue {
    param(
        [Parameter(Mandatory = $false)]
        [AllowNull()]
        [object]$Value
    )

    if ($Value -is [System.Array]) {
        return if ($Value.Count -gt 0) { $Value[0] } else { $null }
    }

    return $Value
}

function Get-RemoteFailureDetails {
    param(
        [Parameter(Mandatory = $true)]
        [System.Exception]$Exception
    )

    $httpStatus = $null
    $errorCode = $null
    $errorMessage = $Exception.Message
    $payloadText = $null

    try {
        if ($null -ne $Exception.Response -and $null -ne $Exception.Response.StatusCode) {
            $statusCodeValue = Get-FirstScalarValue -Value $Exception.Response.StatusCode
            if ($null -ne $statusCodeValue) {
                if ($statusCodeValue -is [System.Enum]) {
                    $httpStatus = [int]$statusCodeValue.value__
                } else {
                    $httpStatus = [int]$statusCodeValue
                }
            }
        }
    } catch {
    }

    try {
        if ($null -ne $Exception.ErrorDetails -and -not [string]::IsNullOrWhiteSpace($Exception.ErrorDetails.Message)) {
            $payloadText = [string]$Exception.ErrorDetails.Message
            $payload = $payloadText | ConvertFrom-Json -ErrorAction Stop
            if ($payload.PSObject.Properties.Name -contains 'error') {
                $errorCode = [string]$payload.error
            }
            if ($payload.PSObject.Properties.Name -contains 'message' -and -not [string]::IsNullOrWhiteSpace([string]$payload.message)) {
                $errorMessage = [string]$payload.message
            }
        }
    } catch {
    }

    if ([string]::IsNullOrWhiteSpace($errorCode)) {
        try {
            if ($Exception.Data.Contains('PipeApiErrorCode')) {
                $errorCode = [string]$Exception.Data['PipeApiErrorCode']
            }
        } catch {
        }
    }

    $failureClass = 'server_error'
    if ($httpStatus -in @(400, 404) -or $errorCode -in @('INVALID_ARGUMENT', 'NOT_FOUND')) {
        $failureClass = 'validation'
    } elseif ($httpStatus -eq 504 -or $errorCode -eq 'EMULE_TIMEOUT' -or $errorMessage -match 'timeout|timed out') {
        $failureClass = 'api_timeout'
    } elseif ($httpStatus -eq 503 -or $errorCode -eq 'EMULE_UNAVAILABLE' -or $errorMessage -match 'pipe .*not connected|pipe connection closed|EPIPE|ENOENT') {
        $failureClass = 'pipe_disconnect'
    } elseif ($errorMessage -match 'stopped responding') {
        $failureClass = 'app_unresponsive'
    }

    return [ordered]@{
        http_status = $httpStatus
        error_code = $errorCode
        error_message = $errorMessage
        failure_class = $failureClass
        payload = $payloadText
    }
}

function Set-ScenarioStep {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Step
    )

    $script:CurrentScenarioStep = $Step
}

function Assert-ScenarioCondition {
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Condition,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-ScenarioApiCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Step,

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

    Set-ScenarioStep -Step $Step
    return Invoke-RemoteJson -Method $Method -Uri $Uri -Token $Token -Body $Body -TimeoutSec $TimeoutSec
}

function Invoke-ExpectedRemoteFailure {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Step,

        [Parameter(Mandatory = $true)]
        [string]$Method,

        [Parameter(Mandatory = $true)]
        [string]$Uri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $false)]
        [object]$Body = $null,

        [Parameter(Mandatory = $false)]
        [int[]]$ExpectedStatus = @(),

        [Parameter(Mandatory = $false)]
        [string[]]$ExpectedErrorCodes = @(),

        [Parameter(Mandatory = $false)]
        [int]$TimeoutSec = 8
    )

    Set-ScenarioStep -Step $Step
    try {
        $null = Invoke-RemoteJson -Method $Method -Uri $Uri -Token $Token -Body $Body -TimeoutSec $TimeoutSec
    } catch {
        $failure = Get-RemoteFailureDetails -Exception $_.Exception
        if ($ExpectedStatus.Count -gt 0) {
            $observedStatus = Get-FirstScalarValue -Value $failure.http_status
            Assert-ScenarioCondition -Condition ($null -ne $observedStatus -and ($ExpectedStatus -contains [int]$observedStatus)) -Message ("Expected HTTP status {0} for step '{1}', got {2}" -f (($ExpectedStatus -join ', ')), $Step, $failure.http_status)
        }
        if ($ExpectedErrorCodes.Count -gt 0) {
            Assert-ScenarioCondition -Condition ($ExpectedErrorCodes -contains [string]$failure.error_code) -Message ("Expected error code {0} for step '{1}', got {2}" -f (($ExpectedErrorCodes -join ', ')), $Step, $failure.error_code)
        }
        return [pscustomobject]$failure
    }

    throw "Expected API failure for step '$Step' but the request succeeded."
}

function Invoke-Scenario {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [ValidateSet('matrix', 'soak')]
        [string]$Kind,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action,

        [Parameter(Mandatory = $false)]
        [switch]$FailFast
    )

    $scenario = [ordered]@{
        name = $Name
        kind = $Kind
        started_at = (Get-Date).ToString('o')
        ended_at = $null
        ok = $false
        step_failed = $null
        http_status = $null
        error_code = $null
        error_message = $null
        failure_class = $null
        result_summary = $null
    }

    $previousScenario = $script:CurrentScenarioContext
    $previousStep = $script:CurrentScenarioStep
    $script:CurrentScenarioContext = $scenario
    $script:CurrentScenarioStep = $null

    try {
        $result = & $Action
        if ($null -ne $result -and ($result.PSObject.Properties.Name -contains 'scenario_ok') -and (-not [bool]$result.scenario_ok)) {
            $scenario.step_failed = $result.step_failed
            $scenario.http_status = $result.http_status
            $scenario.error_code = $result.error_code
            $scenario.error_message = $result.error_message
            $scenario.failure_class = $result.failure_class
            $scenario.result_summary = Get-NormalizedApiValue -Value $result
            if ($FailFast) {
                throw ($scenario.error_message ?? ("Scenario '{0}' failed." -f $Name))
            }
            return $result
        }
        $scenario.ok = $true
        $scenario.result_summary = Get-NormalizedApiValue -Value $result
        return $result
    } catch {
        $failure = Get-RemoteFailureDetails -Exception $_.Exception
        $scenario.step_failed = $script:CurrentScenarioStep
        $scenario.http_status = $failure.http_status
        $scenario.error_code = $failure.error_code
        $scenario.error_message = $failure.error_message
        $scenario.failure_class = $failure.failure_class
        if ($FailFast) {
            throw
        }
        return $null
    } finally {
        $scenario.ended_at = (Get-Date).ToString('o')
        $script:PipeScenarioResults.Add([pscustomobject]$scenario)
        $script:CurrentScenarioContext = $previousScenario
        $script:CurrentScenarioStep = $previousStep
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

    $startedAt = Get-Date
    $transcriptEntry = [ordered]@{
        timestamp = $startedAt.ToString('o')
        scenario = if ($null -ne $script:CurrentScenarioContext) { $script:CurrentScenarioContext.name } else { $null }
        scenario_kind = if ($null -ne $script:CurrentScenarioContext) { $script:CurrentScenarioContext.kind } else { $null }
        step = $script:CurrentScenarioStep
        method = $Method
        uri = $Uri
        payload_summary = Get-NormalizedApiValue -Value $Body
        duration_ms = $null
        ok = $false
        http_status = $null
        error_code = $null
        error_message = $null
        failure_class = $null
        result_summary = $null
    }

    try {
        $result = Invoke-RestMethod @invokeArgs
        $transcriptEntry.ok = $true
        $transcriptEntry.http_status = 200
        $transcriptEntry.result_summary = Get-NormalizedApiValue -Value $result
        return $result
    } catch {
        $failure = Get-RemoteFailureDetails -Exception $_.Exception
        try {
            $_.Exception.Data['PipeApiHttpStatus'] = $failure.http_status
            $_.Exception.Data['PipeApiErrorCode'] = $failure.error_code
            $_.Exception.Data['PipeApiErrorMessage'] = $failure.error_message
            $_.Exception.Data['PipeApiFailureClass'] = $failure.failure_class
        } catch {
        }

        $transcriptEntry.http_status = $failure.http_status
        $transcriptEntry.error_code = $failure.error_code
        $transcriptEntry.error_message = $failure.error_message
        $transcriptEntry.failure_class = $failure.failure_class
        throw
    } finally {
        $transcriptEntry.duration_ms = [int](((Get-Date) - $startedAt).TotalMilliseconds)
        $script:PipeCommandTranscript.Add([pscustomobject]$transcriptEntry)
    }
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

function Wait-NamedPipeAvailability {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PipeName,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutSec
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $pipeEntry = Get-ChildItem '\\.\pipe\' -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $PipeName } |
            Select-Object -First 1
        if ($null -ne $pipeEntry) {
            return $true
        }

        Start-Sleep -Milliseconds 500
    }

    throw "Named pipe '$PipeName' did not appear within $TimeoutSec seconds."
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

function New-SyntheticEd2kLinks {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Count
    )

    if ($Count -le 0) {
        return @()
    }

    $names = @(
        'ubuntu 24.04 LTS [desktop]-x64.iso',
        'odd__name__[test]__(sample)!! 001.bin',
        'semi;colon,comma plus+equals=.avi',
        'many   spaces   and---dashes.txt',
        'parentheses_(demo)_{alpha}_v1.2.zip',
        'mix.of.dots...and__underscores__2026.rar',
        'quote''s sample & reference copy.mp3',
        'hash-tag #release [beta] final!.7z'
    )
    $links = New-Object System.Collections.Generic.List[string]
    for ($index = 0; $index -lt $Count; ++$index) {
        $templateIndex = $index % $names.Count
        $fileName = '{0} [{1:00}]' -f $names[$templateIndex], ($index + 1)
        $escapedName = [System.Uri]::EscapeDataString($fileName)
        $fileSize = 1048576 + ($index * 131072)
        <#*
         * @brief Keep each synthetic transfer hash unique so repeated matrix cycles do not alias prior links.
         #>
        $hash = ('{0:x32}' -f ($index + 1))
        $links.Add("ed2k://|file|$escapedName|$fileSize|$hash|/")
    }

    return @($links)
}

function Get-SuccessfulTransferHashes {
    param(
        [Parameter(Mandatory = $false)]
        [object]$MutationResponse
    )

    if ($null -eq $MutationResponse -or -not ($MutationResponse.PSObject.Properties.Name -contains 'results')) {
        return @()
    }

    $hashes = New-Object System.Collections.Generic.List[string]
    foreach ($result in @($MutationResponse.results)) {
        if ($null -eq $result) {
            continue
        }

        if (($result.PSObject.Properties.Name -contains 'ok') -and $result.ok -and
            ($result.PSObject.Properties.Name -contains 'hash') -and
            -not [string]::IsNullOrWhiteSpace([string]$result.hash)) {
            $hashes.Add([string]$result.hash)
        }
    }

    return @($hashes)
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

function Invoke-TransferMutation {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $true)]
        [ValidateSet('pause', 'resume', 'stop', 'delete')]
        [string]$Action,

        [Parameter(Mandatory = $true)]
        [string[]]$Hashes,

        [Parameter(Mandatory = $false)]
        [bool]$DeleteFiles = $false,

        [Parameter(Mandatory = $false)]
        [string]$StepName = ''
    )

    if ($null -eq $Hashes -or $Hashes.Count -eq 0) {
        return $null
    }

    $body = [ordered]@{
        hashes = @($Hashes)
    }
    if ($Action -eq 'delete') {
        $body.deleteFiles = $DeleteFiles
    }

    if (-not [string]::IsNullOrWhiteSpace($StepName)) {
        return Invoke-ScenarioApiCommand -Step $StepName -Method Post -Uri "$BaseUri/api/v2/transfers/$Action" -Token $Token -Body $body
    }

    return Invoke-RemoteJson -Method Post -Uri "$BaseUri/api/v2/transfers/$Action" -Token $Token -Body $body
}

function Invoke-TransferChurnCycle {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $false)]
        [string]$PrimaryLink,

        [Parameter(Mandatory = $true)]
        [string[]]$SyntheticLinks,

        [Parameter(Mandatory = $true)]
        [int]$LinksPerCycle,

        [Parameter(Mandatory = $true)]
        [int]$CycleIndex,

        [Parameter(Mandatory = $true)]
        [int]$PauseMs
    )

    $linkBatch = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($PrimaryLink)) {
        $linkBatch.Add($PrimaryLink)
    }

    for ($linkIndex = 0; $linkBatch.Count -lt $LinksPerCycle -and $linkIndex -lt $SyntheticLinks.Count; ++$linkIndex) {
        $syntheticIndex = ($CycleIndex + $linkIndex) % $SyntheticLinks.Count
        $linkBatch.Add($SyntheticLinks[$syntheticIndex])
    }

    if ($linkBatch.Count -eq 0) {
        return [pscustomobject][ordered]@{
            links = @()
            add_result = $null
            added_hashes = @()
            pause_result = $null
            resume_result = $null
            stop_result = $null
            delete_result = $null
            transfers_after_add = $null
            transfers_after_delete = $null
            errors = @('no links were available for transfer churn')
        }
    }

    $errors = New-Object System.Collections.Generic.List[string]
    $addResult = $null
    $addedHashes = @()
    $pauseResult = $null
    $resumeResult = $null
    $stopResult = $null
    $deleteResult = $null
    $transfersAfterAdd = $null
    $transfersAfterDelete = $null

    try {
        $addResult = Invoke-ScenarioApiCommand -Step "transfer-churn[$CycleIndex]/transfers/add" -Method Post -Uri "$BaseUri/api/v2/transfers/add" -Token $Token -Body @{
            links = @($linkBatch.ToArray())
        }
        $addedHashes = @(Get-SuccessfulTransferHashes -MutationResponse $addResult)
        $transfersAfterAdd = Invoke-ScenarioApiCommand -Step "transfer-churn[$CycleIndex]/transfers/list after add" -Method Get -Uri "$BaseUri/api/v2/transfers" -Token $Token

        if ($addedHashes.Count -gt 0) {
            if ($PauseMs -gt 0) {
                Start-Sleep -Milliseconds $PauseMs
            }
            $pauseResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'pause' -Hashes $addedHashes -StepName "transfer-churn[$CycleIndex]/transfers/pause"
            if ($PauseMs -gt 0) {
                Start-Sleep -Milliseconds $PauseMs
            }
            $resumeResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'resume' -Hashes $addedHashes -StepName "transfer-churn[$CycleIndex]/transfers/resume"
            if ($PauseMs -gt 0) {
                Start-Sleep -Milliseconds $PauseMs
            }
            $stopResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'stop' -Hashes $addedHashes -StepName "transfer-churn[$CycleIndex]/transfers/stop"
            if ($PauseMs -gt 0) {
                Start-Sleep -Milliseconds $PauseMs
            }
            $deleteResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'delete' -Hashes $addedHashes -DeleteFiles $true -StepName "transfer-churn[$CycleIndex]/transfers/delete"
            $transfersAfterDelete = Invoke-ScenarioApiCommand -Step "transfer-churn[$CycleIndex]/transfers/list after delete" -Method Get -Uri "$BaseUri/api/v2/transfers" -Token $Token
        }
    } catch {
        $errors.Add($_.Exception.Message)
    }

    return [pscustomobject][ordered]@{
        links = @($linkBatch.ToArray())
        add_result = $addResult
        added_hashes = @($addedHashes)
        pause_result = $pauseResult
        resume_result = $resumeResult
        stop_result = $stopResult
        delete_result = $deleteResult
        transfers_after_add = $transfersAfterAdd
        transfers_after_delete = $transfersAfterDelete
        errors = @($errors)
    }
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
                $searchSession = Invoke-ScenarioApiCommand -Step "search-cycle[$Query]/search/start:$method" -Method Post -Uri "$BaseUri/api/v2/search/start" -Token $Token -Body @{
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
                    $searchSnapshot = Invoke-ScenarioApiCommand -Step "search-cycle[$Query]/search/results[$pollCount]" -Method Get -Uri "$BaseUri/api/v2/search/results?search_id=$($searchSession.search_id)" -Token $Token
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
                $stopResult = Invoke-ScenarioApiCommand -Step "search-cycle[$Query]/search/stop" -Method Post -Uri "$BaseUri/api/v2/search/stop" -Token $Token -Body @{
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

    $step = 'stats/global'
    try {
        $stats = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/stats/global" -Token $Token
        $step = 'transfers/list'
        $transfers = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/transfers" -Token $Token
        $step = 'log/get'
        $recentLog = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/log?limit=40" -Token $Token
        $step = 'sample transfer hashes'
        $sampledTransferHashes = @(Get-SampledTransferHashes -TransfersResponse $transfers -Count $TransferProbeCount)

        $transferDetails = New-Object System.Collections.Generic.List[object]
        $transferSources = New-Object System.Collections.Generic.List[object]
        foreach ($hash in $sampledTransferHashes) {
            $step = "transfers/get:$hash"
            $transferDetails.Add((Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/transfers/$hash" -Token $Token))
            $step = "transfers/sources:$hash"
            $transferSources.Add([pscustomobject][ordered]@{
                hash = $hash
                sources = (Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/transfers/$hash/sources" -Token $Token)
            })
        }

        $uploadSnapshots = New-Object System.Collections.Generic.List[object]
        for ($uploadProbeIndex = 0; $uploadProbeIndex -lt $UploadProbeCount; ++$uploadProbeIndex) {
            $step = "uploads/list[$uploadProbeIndex]"
            $uploadList = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/uploads/list" -Token $Token
            $step = "uploads/queue[$uploadProbeIndex]"
            $uploadQueue = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/uploads/queue" -Token $Token
            $uploadSnapshots.Add([pscustomobject][ordered]@{
                list = $uploadList
                queue = $uploadQueue
            })
        }

        $extraStatsBursts = New-Object System.Collections.Generic.List[object]
        for ($burstIndex = 0; $burstIndex -lt $ExtraStatsBurstsPerPoll; ++$burstIndex) {
            $step = "stats/global burst[$burstIndex]"
            $burstStats = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/stats/global" -Token $Token
            $step = "log/get burst[$burstIndex]"
            $burstLog = Invoke-ScenarioApiCommand -Step $step -Method Get -Uri "$BaseUri/api/v2/log?limit=20" -Token $Token
            $extraStatsBursts.Add([pscustomobject][ordered]@{
                stats = $burstStats
                recent_log = $burstLog
            })
        }
    } catch {
        throw "Invoke-StressProbe failed at '$step': $($_.Exception.Message)"
    }

    return [pscustomobject][ordered]@{
        stats = $stats
        transfers = $transfers
        recent_log = $recentLog
        sampled_transfer_hashes = @($sampledTransferHashes)
        transfer_details = @($transferDetails.ToArray())
        transfer_sources = @($transferSources.ToArray())
        upload_snapshots = @($uploadSnapshots.ToArray())
        extra_stats_bursts = @($extraStatsBursts.ToArray())
    }
}

function Get-ScenarioSummary {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('matrix', 'soak')]
        [string]$Kind
    )

    $scenarios = @($script:PipeScenarioResults | Where-Object { $_.kind -eq $Kind })
    $failedScenarios = @($scenarios | Where-Object { -not $_.ok } |
        Select-Object name, step_failed, http_status, error_code, error_message, failure_class)

    return [ordered]@{
        kind = $Kind
        scenario_count = $scenarios.Count
        passed = @($scenarios | Where-Object { $_.ok }).Count
        failed = $failedScenarios.Count
        failed_scenarios = @($failedScenarios)
        scenarios = @($scenarios)
    }
}

function Invoke-PipeScenarioMatrix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $true)]
        [string[]]$SearchQueries,

        [Parameter(Mandatory = $true)]
        [int]$SearchWaitSec,

        [Parameter(Mandatory = $false)]
        [string]$FallbackLink,

        [Parameter(Mandatory = $true)]
        [string[]]$SyntheticLinks,

        [Parameter(Mandatory = $true)]
        [int]$MatrixRepeatCount,

        [Parameter(Mandatory = $true)]
        [int]$TransferPauseMs,

        [Parameter(Mandatory = $false)]
        [switch]$FailFast
    )

    $selectedDownloadLink = $FallbackLink
    $repeatCount = [Math]::Max($MatrixRepeatCount, 1)

    for ($repeatIndex = 0; $repeatIndex -lt $repeatCount; ++$repeatIndex) {
        $query = $SearchQueries[$repeatIndex % $SearchQueries.Count]
        $matrixSearchResult = Invoke-Scenario -Name ("matrix/search-valid[{0}]" -f ($repeatIndex + 1)) -Kind 'matrix' -FailFast:$FailFast -Action {
            $searchSession = Invoke-ScenarioApiCommand -Step 'search/start' -Method Post -Uri "$BaseUri/api/v2/search/start" -Token $Token -Body @{
                query = $query
                method = 'global'
            }
            Assert-ScenarioCondition -Condition ($null -ne $searchSession -and $null -ne $searchSession.search_id) -Message 'search/start did not return a search_id'

            $deadline = (Get-Date).AddSeconds([Math]::Min([Math]::Max($SearchWaitSec, 5), 20))
            $pollCount = 0
            $searchSnapshot = $null
            $candidateDownloadLink = $null
            while ((Get-Date) -lt $deadline) {
                $pollCount += 1
                $searchSnapshot = Invoke-ScenarioApiCommand -Step ("search/results[{0}]" -f $pollCount) -Method Get -Uri "$BaseUri/api/v2/search/results?search_id=$($searchSession.search_id)" -Token $Token
                if ($null -ne $searchSnapshot -and ($searchSnapshot.PSObject.Properties.Name -contains 'results')) {
                    foreach ($result in @($searchSnapshot.results)) {
                        if ($null -eq $result) {
                            continue
                        }

                        $candidateDownloadLink = Build-Ed2kLinkFromResult -SearchResult $result
                        if (-not [string]::IsNullOrWhiteSpace($candidateDownloadLink)) {
                            break
                        }
                    }
                }

                if (-not [string]::IsNullOrWhiteSpace($candidateDownloadLink)) {
                    break
                }

                Start-Sleep -Seconds 5
            }

            $stopResult = Invoke-ScenarioApiCommand -Step 'search/stop' -Method Post -Uri "$BaseUri/api/v2/search/stop" -Token $Token -Body @{
                search_id = $searchSession.search_id
            }

            return [pscustomobject][ordered]@{
                query = $query
                search_id = $searchSession.search_id
                poll_count = $pollCount
                result_count = if ($null -ne $searchSnapshot -and ($searchSnapshot.PSObject.Properties.Name -contains 'results')) { @($searchSnapshot.results).Count } else { 0 }
                selected_download_link = $candidateDownloadLink
                stop_result = $stopResult
            }
        }
        if ($null -ne $matrixSearchResult -and -not [string]::IsNullOrWhiteSpace($matrixSearchResult.selected_download_link)) {
            $selectedDownloadLink = [string]$matrixSearchResult.selected_download_link
        }

        $null = Invoke-Scenario -Name ("matrix/search-invalid-empty[{0}]" -f ($repeatIndex + 1)) -Kind 'matrix' -FailFast:$FailFast -Action {
            return Invoke-ExpectedRemoteFailure -Step 'search/start empty query' -Method Post -Uri "$BaseUri/api/v2/search/start" -Token $Token -Body @{
                query = ''
                method = 'global'
            } -ExpectedStatus @(400) -ExpectedErrorCodes @('INVALID_ARGUMENT')
        }

        $null = Invoke-Scenario -Name ("matrix/search-invalid-method[{0}]" -f ($repeatIndex + 1)) -Kind 'matrix' -FailFast:$FailFast -Action {
            return Invoke-ExpectedRemoteFailure -Step 'search/start invalid method' -Method Post -Uri "$BaseUri/api/v2/search/start" -Token $Token -Body @{
                query = $query
                method = 'broken'
            } -ExpectedStatus @(400) -ExpectedErrorCodes @('INVALID_ARGUMENT')
        }

        $transferResult = Invoke-Scenario -Name ("matrix/transfers-roundtrip[{0}]" -f ($repeatIndex + 1)) -Kind 'matrix' -FailFast:$FailFast -Action {
            $linkBatch = New-Object System.Collections.Generic.List[string]
            if (-not [string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
                $linkBatch.Add($selectedDownloadLink)
            }
            for ($linkIndex = 0; $linkBatch.Count -lt 3 -and $linkIndex -lt $SyntheticLinks.Count; ++$linkIndex) {
                $syntheticIndex = ($repeatIndex * 3 + $linkIndex) % $SyntheticLinks.Count
                $linkBatch.Add($SyntheticLinks[$syntheticIndex])
            }
            Assert-ScenarioCondition -Condition ($linkBatch.Count -gt 0) -Message 'no transfer links were available for the matrix roundtrip'

            $addResult = Invoke-ScenarioApiCommand -Step 'transfers/add' -Method Post -Uri "$BaseUri/api/v2/transfers/add" -Token $Token -Body @{
                links = @($linkBatch.ToArray())
            }
            $addedHashes = @(Get-SuccessfulTransferHashes -MutationResponse $addResult)
            Assert-ScenarioCondition -Condition ($addedHashes.Count -gt 0) -Message 'transfers/add did not return any successful hashes'

            $transfersList = Invoke-ScenarioApiCommand -Step 'transfers/list' -Method Get -Uri "$BaseUri/api/v2/transfers" -Token $Token
            $primaryHash = $addedHashes[0]
            $transferDetail = Invoke-ScenarioApiCommand -Step 'transfers/get' -Method Get -Uri "$BaseUri/api/v2/transfers/$primaryHash" -Token $Token
            Assert-ScenarioCondition -Condition ([string]$transferDetail.hash -eq $primaryHash) -Message 'transfers/get did not return the requested hash'

            $transferSources = Invoke-ScenarioApiCommand -Step 'transfers/sources' -Method Get -Uri "$BaseUri/api/v2/transfers/$primaryHash/sources" -Token $Token
            $pauseResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'pause' -Hashes $addedHashes -StepName 'transfers/pause'
            $resumeResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'resume' -Hashes $addedHashes -StepName 'transfers/resume'
            $stopResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'stop' -Hashes $addedHashes -StepName 'transfers/stop'
            $deleteResult = Invoke-TransferMutation -BaseUri $BaseUri -Token $Token -Action 'delete' -Hashes $addedHashes -DeleteFiles $true -StepName 'transfers/delete'
            $transfersAfterDelete = Invoke-ScenarioApiCommand -Step 'transfers/list after delete' -Method Get -Uri "$BaseUri/api/v2/transfers" -Token $Token

            Assert-ScenarioCondition -Condition (@($pauseResult.results).Count -eq $addedHashes.Count) -Message 'transfers/pause did not return one result row per hash'
            Assert-ScenarioCondition -Condition (@($deleteResult.results).Count -eq $addedHashes.Count) -Message 'transfers/delete did not return one result row per hash'

            return [pscustomobject][ordered]@{
                links = @($linkBatch.ToArray())
                added_hashes = @($addedHashes)
                transfers_before_delete = $transfersList
                transfer_detail = $transferDetail
                transfer_sources = $transferSources
                pause_result = $pauseResult
                resume_result = $resumeResult
                stop_result = $stopResult
                delete_result = $deleteResult
                transfers_after_delete = $transfersAfterDelete
            }
        }
        if ($null -ne $transferResult -and @($transferResult.added_hashes).Count -gt 0) {
            $selectedDownloadLink = $transferResult.links[0]
        }

        $null = Invoke-Scenario -Name ("matrix/transfers-invalid-hash[{0}]" -f ($repeatIndex + 1)) -Kind 'matrix' -FailFast:$FailFast -Action {
            return Invoke-ExpectedRemoteFailure -Step 'transfers/get invalid hash' -Method Get -Uri "$BaseUri/api/v2/transfers/not-a-hash" -Token $Token -ExpectedStatus @(400) -ExpectedErrorCodes @('INVALID_ARGUMENT')
        }

        $null = Invoke-Scenario -Name ("matrix/transfers-not-found[{0}]" -f ($repeatIndex + 1)) -Kind 'matrix' -FailFast:$FailFast -Action {
            return Invoke-ExpectedRemoteFailure -Step 'transfers/get missing hash' -Method Get -Uri "$BaseUri/api/v2/transfers/feedfacefeedfacefeedfacefeedface" -Token $Token -ExpectedStatus @(404) -ExpectedErrorCodes @('NOT_FOUND')
        }
    }

    return [pscustomobject][ordered]@{
        selected_download_link = $selectedDownloadLink
    }
}

function Invoke-PipeSoakScenario {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUri,

        [Parameter(Mandatory = $true)]
        [string]$Token,

        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath,

        [Parameter(Mandatory = $true)]
        [string[]]$ConfiguredSearchQueries,

        [Parameter(Mandatory = $true)]
        [int]$SearchWaitSec,

        [Parameter(Mandatory = $true)]
        [int]$SearchCycleCount,

        [Parameter(Mandatory = $true)]
        [int]$SearchCyclePauseSec,

        [Parameter(Mandatory = $true)]
        [int]$MonitorSec,

        [Parameter(Mandatory = $true)]
        [int]$PollSec,

        [Parameter(Mandatory = $true)]
        [int]$TransferProbeCount,

        [Parameter(Mandatory = $true)]
        [int]$UploadProbeCount,

        [Parameter(Mandatory = $true)]
        [int]$ExtraStatsBurstsPerPoll,

        [Parameter(Mandatory = $true)]
        [int]$TransferChurnCycles,

        [Parameter(Mandatory = $true)]
        [int]$TransfersPerChurnCycle,

        [Parameter(Mandatory = $true)]
        [int]$TransferChurnPauseMs,

        [Parameter(Mandatory = $true)]
        [string[]]$SyntheticLinks,

        [Parameter(Mandatory = $true)]
        [string]$LogDir,

        [Parameter(Mandatory = $false)]
        [string]$InitialSelectedDownloadLink,

        [Parameter(Mandatory = $false)]
        [string]$FallbackLink,

        [Parameter(Mandatory = $true)]
        [string]$ArtifactDir
    )

    return Invoke-Scenario -Name 'soak/mixed-search-transfer' -Kind 'soak' -Action {
        $selectedDownloadLink = if (-not [string]::IsNullOrWhiteSpace($InitialSelectedDownloadLink)) { $InitialSelectedDownloadLink } else { $FallbackLink }
        $searchCycles = New-Object System.Collections.Generic.List[object]
        $transferChurnHistory = New-Object System.Collections.Generic.List[object]
        $samples = New-Object System.Collections.Generic.List[object]
        $transferAddResult = $null
        $completedTransferChurnCycles = 0
        $consecutiveApiFailures = 0
        $consecutiveNonResponding = 0
        $freezeReason = $null
        $dumpPath = $null
        $lastApiFailure = $null

        for ($searchCycleIndex = 0; $searchCycleIndex -lt $SearchCycleCount; ++$searchCycleIndex) {
            $query = $ConfiguredSearchQueries[$searchCycleIndex % $ConfiguredSearchQueries.Count]
            $searchCycle = Invoke-SearchCycle -BaseUri $BaseUri -Token $Token -Query $query -WaitSec $SearchWaitSec -FallbackLink $FallbackLink
            $searchCycles.Add($searchCycle)

            if ([string]::IsNullOrWhiteSpace($selectedDownloadLink) -and -not [string]::IsNullOrWhiteSpace($searchCycle.selected_download_link)) {
                $selectedDownloadLink = $searchCycle.selected_download_link
            }

            if ($searchCycleIndex + 1 -lt $SearchCycleCount -and $SearchCyclePauseSec -gt 0) {
                Start-Sleep -Seconds $SearchCyclePauseSec
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($selectedDownloadLink)) {
            $transferAddResult = Invoke-ScenarioApiCommand -Step 'soak/bootstrap/transfers/add' -Method Post -Uri "$BaseUri/api/v2/transfers/add" -Token $Token -Body @{
                links = @($selectedDownloadLink)
            }
        }

        $deadline = (Get-Date).AddSeconds($MonitorSec)
        while ((Get-Date) -lt $deadline) {
            $runningProcess = Get-MatchingProcess -ProcessName 'emule' -ExecutablePath $ExecutablePath
            if ($null -eq $runningProcess) {
                $freezeReason = 'eMule process exited during soak monitoring'
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
            $transferChurn = $null
            $transferChurnError = $null
            $apiError = $null
            $apiErrorDetail = $null
            $apiErrorCode = $null
            $apiErrorStatus = $null
            $apiFailureClass = $null
            $apiStartedAt = Get-Date
            try {
                if ($completedTransferChurnCycles -lt $TransferChurnCycles) {
                    $transferChurn = Invoke-TransferChurnCycle -BaseUri $BaseUri -Token $Token -PrimaryLink $selectedDownloadLink -SyntheticLinks $SyntheticLinks -LinksPerCycle $TransfersPerChurnCycle -CycleIndex $completedTransferChurnCycles -PauseMs $TransferChurnPauseMs
                    $transferChurnHistory.Add($transferChurn)
                    $completedTransferChurnCycles += 1
                    $transferChurnErrors = @($transferChurn.errors)
                    if ($transferChurnErrors.Count -gt 0) {
                        $transferChurnError = ($transferChurnErrors -join '; ')
                    }
                }
                $stressProbe = Invoke-StressProbe -BaseUri $BaseUri -Token $Token -TransferProbeCount $TransferProbeCount -UploadProbeCount $UploadProbeCount -ExtraStatsBurstsPerPoll $ExtraStatsBurstsPerPoll
                $stats = $stressProbe.stats
                $transfers = $stressProbe.transfers
                $recentLog = $stressProbe.recent_log
                $consecutiveApiFailures = 0
                $lastApiFailure = $null
            } catch {
                $apiError = $_.Exception.Message
                $apiErrorDetail = $_ | Out-String
                $lastApiFailure = Get-RemoteFailureDetails -Exception $_.Exception
                $apiErrorCode = $lastApiFailure.error_code
                $apiErrorStatus = $lastApiFailure.http_status
                $apiFailureClass = $lastApiFailure.failure_class
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
                api_error_code = $apiErrorCode
                api_error_status = $apiErrorStatus
                api_failure_class = $apiFailureClass
                transfer_churn = $transferChurn
                transfer_churn_error = $transferChurnError
                stats = $stats
                transfers = $transfers
                recent_log = $recentLog
                stress_probe = $stressProbe
                disk_logs = Get-LogSnapshot -LogDir $LogDir
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
            $runningProcess = Get-MatchingProcess -ProcessName 'emule' -ExecutablePath $ExecutablePath
            if ($null -ne $runningProcess) {
                $dumpPath = Join-Path $ArtifactDir ("emule-freeze-{0}.dmp" -f $runningProcess.Id)
                $dumpPath = Save-HangDump -ProcessId $runningProcess.Id -DumpPath $dumpPath
            }
        }

        return [pscustomobject][ordered]@{
            scenario_ok = [string]::IsNullOrWhiteSpace($freezeReason)
            step_failed = if (-not [string]::IsNullOrWhiteSpace($freezeReason)) { $script:CurrentScenarioStep } else { $null }
            http_status = if ($null -ne $lastApiFailure) { $lastApiFailure.http_status } else { $null }
            error_code = if ($null -ne $lastApiFailure) { $lastApiFailure.error_code } else { $null }
            error_message = if (-not [string]::IsNullOrWhiteSpace($freezeReason)) { $freezeReason } else { $null }
            failure_class = if ($null -ne $lastApiFailure) { $lastApiFailure.failure_class } elseif (-not [string]::IsNullOrWhiteSpace($freezeReason)) { 'app_unresponsive' } else { $null }
            search_cycles = @($searchCycles.ToArray())
            selected_download_link = $selectedDownloadLink
            transfer_add_result = $transferAddResult
            transfer_churn_cycles = @($transferChurnHistory.ToArray())
            sample_count = $samples.Count
            runtime_samples = @($samples.ToArray())
            freeze_reason = $freezeReason
            dump_path = $dumpPath
        }
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
$testsRoot = Get-NormalizedPath -Path (Join-Path $workspaceRoot '..\eMule-build-tests')
$remoteRoot = Get-NormalizedPath -Path (Join-Path $workspaceRoot '..\eMule-remote')
$buildScriptPath = Join-Path $workspaceRoot '23-build-emule-debug-incremental.cmd'
$buildExePath = Join-Path $repoRoot 'srchybrid\x64\Debug\emule.exe'
$launchedExePath = Join-Path $repoRoot 'srchybrid\x64\Debug\eMule_v072_harness.exe'
$launchedProcessName = [System.IO.Path]::GetFileNameWithoutExtension($launchedExePath)
$remoteEntryPoint = Join-Path $remoteRoot 'dist\server\index.js'
$sessionStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$defaultSeedRoot = Join-Path $testsRoot 'manifests\live-profile-seed'
$seedRoot = if ([string]::IsNullOrWhiteSpace($SeedRoot)) {
    Get-NormalizedPath -Path $defaultSeedRoot
} else {
    Get-NormalizedPath -Path $SeedRoot
}
$profileBaseRoot = Get-NormalizedPath -Path $ProfileRoot
$profileRunsRoot = Join-Path $profileBaseRoot 'runs'
$profileRoot = Join-Path $profileRunsRoot $sessionStamp
$profileLatestRoot = Join-Path $profileBaseRoot 'latest'
$configDir = Join-Path $profileRoot 'config'
$logDir = Join-Path $profileRoot 'logs'
$preferencesPath = Join-Path $configDir 'preferences.ini'
$artifactDir = Join-Path (Join-Path $workspaceRoot 'logs') ($sessionStamp + '-pipe-live-session')
$artifactLatestDir = Join-Path (Join-Path $workspaceRoot 'logs') 'pipe-live-session-latest'
$artifactManifestPath = Join-Path $artifactDir 'session-manifest.json'
$manifestPaths = @($artifactManifestPath)
if (-not [string]::IsNullOrWhiteSpace($SessionManifestPath)) {
    $manifestPaths += (Get-NormalizedPath -Path $SessionManifestPath)
}
$summaryPath = Join-Path $artifactDir 'session-summary.txt'
$remoteStdOutPath = Join-Path $artifactDir 'remote-stdout.log'
$remoteStdErrPath = Join-Path $artifactDir 'remote-stderr.log'
$crtLogPath = Join-Path $logDir 'eMule CRT Debug Log.log'
$baseUri = "http://127.0.0.1:$RemotePort"

Ensure-Directory -Path $artifactDir
Ensure-Directory -Path $profileBaseRoot
Ensure-Directory -Path $profileRunsRoot

if (-not (Test-Path -LiteralPath $seedRoot -PathType Container)) {
    throw "Seed root '$seedRoot' does not exist."
}

Initialize-LiveSessionProfile -SeedRoot $seedRoot -WorkingProfileRoot $profileRoot

$bindInterface = Get-NetAdapter -Name $BindInterfaceName -ErrorAction SilentlyContinue
if ($null -eq $bindInterface) {
    throw "Bind interface '$BindInterfaceName' was not found."
}

if (-not $SkipBuild) {
    & $buildScriptPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath $buildExePath -PathType Leaf)) {
    throw "Debug executable '$buildExePath' does not exist."
}

if (-not (Test-Path -LiteralPath $remoteEntryPoint -PathType Leaf)) {
    throw "Remote entry point '$remoteEntryPoint' does not exist."
}

Set-LiveSessionPreferences -PreferencesPath $preferencesPath -ProfileRoot $profileRoot -BindInterfaceName $BindInterfaceName

$profileLatestPublished = $false
$profileLatestError = $null
try {
    Publish-LatestDirectoryPointer -TargetDirectory $profileRoot -LatestDirectory $profileLatestRoot
    $profileLatestPublished = $true
} catch {
    $profileLatestError = $_.Exception.Message
}

$stoppedHarnessProcessId = Stop-MatchingProcess -ProcessName $launchedProcessName -ExecutablePath $launchedExePath
$stoppedRemoteProcessIds = Stop-ListeningProcessByPort -Port $RemotePort
$stoppedRemoteSidecarProcessIds = Stop-ProcessesByCommandLinePattern -ProcessName 'node' -CommandPattern $remoteEntryPoint

Publish-HarnessExecutableCopy -SourceExecutablePath $buildExePath -HarnessExecutablePath $launchedExePath

$manifest = [ordered]@{
    helper = 'helper-runtime-pipe-live-session.ps1'
    started_at = (Get-Date).ToString('o')
    session_stamp = $sessionStamp
    build_exe_path = $buildExePath
    launched_exe_path = $launchedExePath
    remote_entry_point = $remoteEntryPoint
    seed_root = $seedRoot
    profile_base_root = $profileBaseRoot
    profile_runs_root = $profileRunsRoot
    profile_root = $profileRoot
    profile_latest_root = $profileLatestRoot
    profile_latest_published = $profileLatestPublished
    profile_latest_error = $profileLatestError
    config_dir = $configDir
    log_dir = $logDir
    artifact_dir = $artifactDir
    artifact_latest_dir = $artifactLatestDir
    launch_only = [bool]$LaunchOnly
    keep_running = [bool]$KeepRunning
    pipe_name = '\\.\pipe\emule-api'
    base_uri = $baseUri
    remote_token = $RemoteToken
    bind_interface_name = $BindInterfaceName
    bind_interface_description = $bindInterface.InterfaceDescription
    search_query = $SearchQuery
    stress_queries = @($StressQueries)
    scenario_profile = $ScenarioProfile
    matrix_repeat_count = $MatrixRepeatCount
    strict_matrix = [bool]$StrictMatrix
    search_wait_sec = $SearchWaitSec
    search_cycle_count = $SearchCycleCount
    search_cycle_pause_sec = $SearchCyclePauseSec
    monitor_sec = $MonitorSec
    poll_sec = $PollSec
    transfer_probe_count = $TransferProbeCount
    upload_probe_count = $UploadProbeCount
    extra_stats_bursts_per_poll = $ExtraStatsBurstsPerPoll
    transfer_churn_cycles = $TransferChurnCycles
    transfers_per_churn_cycle = $TransfersPerChurnCycle
    transfer_churn_pause_ms = $TransferChurnPauseMs
    pipe_warmup_sec = $PipeWarmupSec
    remote_port = $RemotePort
    stopped_existing_harness_process_id = $stoppedHarnessProcessId
    stopped_existing_remote_process_ids = @($stoppedRemoteProcessIds)
    stopped_existing_remote_sidecar_process_ids = @($stoppedRemoteSidecarProcessIds)
    launch_status = 'starting'
}
Write-SessionManifest -Manifest $manifest -Paths $manifestPaths

$startedProcesses = New-Object System.Collections.Generic.List[object]
$cleanupResult = [ordered]@{
    attempts = 0
    success = $true
    stopped_process_ids = @()
    leftover_processes = @()
    leftover_process_ids = @()
    error = $null
}

$startProcessArgs = @{
    FilePath = $launchedExePath
    WorkingDirectory = (Split-Path -Parent $launchedExePath)
    ArgumentList = @('-assertfile', '-c', $profileRoot)
    PassThru = $true
}
$emuleProcess = Start-Process @startProcessArgs
$startedProcesses.Add([ordered]@{
    kind = 'emule'
    process_id = [int]$emuleProcess.Id
    process_name = $launchedProcessName
    executable_path = $launchedExePath
    command_pattern = $null
    match_kind = 'path'
})

$nodePath = (Get-Command node -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
if ([string]::IsNullOrWhiteSpace($nodePath)) {
    throw 'Node.js was not found on PATH.'
}

$manifest.process_id = $emuleProcess.Id
$manifest.started_processes = @($startedProcesses.ToArray())
$manifest.started_process_ids = @($startedProcesses | ForEach-Object { [int]$_.process_id })
$manifest.launch_status = 'waiting_for_pipe'
Write-SessionManifest -Manifest $manifest -Paths $manifestPaths

$serversStatus = $null
$kadStatus = $null
$health = $null
$fallbackLink = $null
$configuredSearchQueries = $null
$selectedDownloadLink = $null
$matrixResult = $null
$soakResult = $null
$matrixSummary = $null
$soakSummary = $null
$syntheticLinks = @(New-SyntheticEd2kLinks -Count ([Math]::Max($TransfersPerChurnCycle * 2, 8)))
$remoteHandle = $null
$sessionFailure = $null
$sessionFailureDetail = $null

try {
    $null = Wait-NamedPipeAvailability -PipeName 'emule-api' -TimeoutSec 60
    if ($PipeWarmupSec -gt 0) {
        Start-Sleep -Seconds $PipeWarmupSec
    }
    $remoteHandle = Start-RedirectedProcess -FilePath $nodePath -Arguments @($remoteEntryPoint) -WorkingDirectory $remoteRoot -StdOutPath $remoteStdOutPath -StdErrPath $remoteStdErrPath -Environment @{
        EMULE_REMOTE_HOST = '127.0.0.1'
        EMULE_REMOTE_PORT = $RemotePort.ToString()
        EMULE_REMOTE_TOKEN = $RemoteToken
        EMULE_REMOTE_TIMEOUT_MS = '5000'
        EMULE_REMOTE_RECONNECT_MS = '1000'
    }
    $startedProcesses.Add([ordered]@{
        kind = 'remote_sidecar'
        process_id = [int]$remoteHandle.Process.Id
        process_name = 'node'
        executable_path = $nodePath
        command_pattern = $remoteEntryPoint
        match_kind = 'command_line'
    })
    $manifest.remote_process_id = $remoteHandle.Process.Id
    $manifest.started_processes = @($startedProcesses.ToArray())
    $manifest.started_process_ids = @($startedProcesses | ForEach-Object { [int]$_.process_id })
    $manifest.launch_status = 'running'
    Write-SessionManifest -Manifest $manifest -Paths $manifestPaths

    $health = Wait-RemoteHealth -BaseUri $baseUri -TimeoutSec 90
    $fallbackLink = Get-FallbackEd2kLink -ConfigDir $configDir
    $configuredSearchQueries = @(Get-ConfiguredSearchQueries -PrimaryQuery $SearchQuery -AdditionalQueries $StressQueries)

    if ($LaunchOnly) {
        $manifest.launch_status = 'launch_only_ready'
        $manifest.remote_health = $health
        Write-SessionManifest -Manifest $manifest -Paths $manifestPaths
    } else {

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

        if ($ScenarioProfile -ne 'soak') {
            $matrixResult = Invoke-PipeScenarioMatrix -BaseUri $baseUri -Token $RemoteToken -SearchQueries $configuredSearchQueries -SearchWaitSec $SearchWaitSec -FallbackLink $fallbackLink -SyntheticLinks $syntheticLinks -MatrixRepeatCount $MatrixRepeatCount -TransferPauseMs $TransferChurnPauseMs -FailFast:$StrictMatrix
            if ($null -ne $matrixResult -and -not [string]::IsNullOrWhiteSpace($matrixResult.selected_download_link)) {
                $selectedDownloadLink = $matrixResult.selected_download_link
            }
            $matrixSummary = Get-ScenarioSummary -Kind 'matrix'
            if ($matrixSummary.failed -gt 0) {
                $sessionFailure = "{0} matrix scenario(s) failed." -f $matrixSummary.failed
                $sessionFailureDetail = ($matrixSummary.failed_scenarios | ConvertTo-Json -Depth 8)
            }
        }

        if ([string]::IsNullOrWhiteSpace($sessionFailure) -and $ScenarioProfile -ne 'matrix') {
            $soakResult = Invoke-PipeSoakScenario -BaseUri $baseUri -Token $RemoteToken -ExecutablePath $launchedExePath -ConfiguredSearchQueries $configuredSearchQueries -SearchWaitSec $SearchWaitSec -SearchCycleCount $SearchCycleCount -SearchCyclePauseSec $SearchCyclePauseSec -MonitorSec $MonitorSec -PollSec $PollSec -TransferProbeCount $TransferProbeCount -UploadProbeCount $UploadProbeCount -ExtraStatsBurstsPerPoll $ExtraStatsBurstsPerPoll -TransferChurnCycles $TransferChurnCycles -TransfersPerChurnCycle $TransfersPerChurnCycle -TransferChurnPauseMs $TransferChurnPauseMs -SyntheticLinks $syntheticLinks -LogDir $logDir -InitialSelectedDownloadLink $selectedDownloadLink -FallbackLink $fallbackLink -ArtifactDir $artifactDir
            $soakSummary = Get-ScenarioSummary -Kind 'soak'
            if ($null -ne $soakResult -and -not [string]::IsNullOrWhiteSpace($soakResult.selected_download_link)) {
                $selectedDownloadLink = $soakResult.selected_download_link
            }
            if ($null -ne $soakResult -and -not [bool]$soakResult.scenario_ok) {
                $sessionFailure = [string]$soakResult.error_message
                $sessionFailureDetail = ($soakResult | ConvertTo-Json -Depth 12)
            }
        }
    }
} catch {
    $sessionFailure = $_.Exception.Message
    $sessionFailureDetail = $_ | Out-String
} finally {
    if (-not $KeepRunning) {
        try {
            <#*
             * @brief Retry targeted teardown and fail the run if any harness-launched process is still present afterward.
             #>
            $cleanupResult = Invoke-TrackedProcessCleanup -TrackedProcesses @($startedProcesses.ToArray()) -AttemptCount 2 -WaitMilliseconds 1000
            if (-not [bool]$cleanupResult.success) {
                $cleanupFailure = "Harness cleanup left behind $($cleanupResult.leftover_process_ids.Count) launched process(es)."
                if ([string]::IsNullOrWhiteSpace($sessionFailure)) {
                    $sessionFailure = $cleanupFailure
                    $sessionFailureDetail = $cleanupResult | ConvertTo-Json -Depth 12
                } else {
                    $sessionFailureDetail = @(
                        $sessionFailureDetail
                        'Cleanup failure:'
                        ($cleanupResult | ConvertTo-Json -Depth 12)
                    ) -join [Environment]::NewLine
                }
            }
        } catch {
            $cleanupResult = [ordered]@{
                attempts = 0
                success = $false
                stopped_process_ids = @()
                leftover_processes = @()
                leftover_process_ids = @()
                error = $_.Exception.Message
            }
            if ([string]::IsNullOrWhiteSpace($sessionFailure)) {
                $sessionFailure = 'Harness cleanup raised an unexpected error.'
                $sessionFailureDetail = $_ | Out-String
            } else {
                $sessionFailureDetail = @(
                    $sessionFailureDetail
                    'Cleanup exception:'
                    ($_ | Out-String)
                ) -join [Environment]::NewLine
            }
        }
    }
}

if ($LaunchOnly) {
    $manifest.finished_at = (Get-Date).ToString('o')
    $manifest.latest_artifact_published = $false
    $manifest.latest_artifact_error = $null
    $manifest.started_processes = @($startedProcesses.ToArray())
    $manifest.started_process_ids = @($startedProcesses | ForEach-Object { [int]$_.process_id })
    $manifest.cleanup_requested = (-not [bool]$KeepRunning)
    $manifest.cleanup_attempts = $cleanupResult.attempts
    $manifest.cleanup_success = [bool]$cleanupResult.success
    $manifest.cleanup_stopped_process_ids = @($cleanupResult.stopped_process_ids)
    $manifest.leftover_process_ids = @($cleanupResult.leftover_process_ids)
    $manifest.leftover_processes = @($cleanupResult.leftover_processes)
    $manifest.cleanup_error = $cleanupResult.error
    $manifest.session_failure = $sessionFailure
    $manifest.session_failure_detail = $sessionFailureDetail
    Write-SessionManifest -Manifest $manifest -Paths $manifestPaths
    Write-Output "Pipe live session artifact directory: $artifactDir"
    if (-not [string]::IsNullOrWhiteSpace($sessionFailure)) {
        Write-Error $sessionFailure
        exit 1
    }
    return
}

$finalLogSnapshot = Get-LogSnapshot -LogDir $logDir
$runtimeSamples = @()
$freezeReason = $null
$dumpPath = $null
$searchCycles = @()
$transferAddResult = $null
$transferChurnCycles = @()
if ($null -ne $soakResult) {
    $runtimeSamples = @($soakResult.runtime_samples)
    $freezeReason = $soakResult.freeze_reason
    $dumpPath = $soakResult.dump_path
    $searchCycles = @($soakResult.search_cycles)
    $transferAddResult = $soakResult.transfer_add_result
    $transferChurnCycles = @($soakResult.transfer_churn_cycles)
}

if ($null -eq $matrixSummary) {
    $matrixSummary = Get-ScenarioSummary -Kind 'matrix'
}
if ($null -eq $soakSummary) {
    $soakSummary = Get-ScenarioSummary -Kind 'soak'
}

$runtimeSamples | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $artifactDir 'runtime-samples.json') -Encoding utf8
$finalLogSnapshot | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $artifactDir 'log-snapshot.json') -Encoding utf8
$script:PipeScenarioResults | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $artifactDir 'scenario-results.json') -Encoding utf8
$script:PipeCommandTranscript | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $artifactDir 'command-transcript.json') -Encoding utf8
$matrixSummary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $artifactDir 'matrix-summary.json') -Encoding utf8
$soakSummary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $artifactDir 'soak-summary.json') -Encoding utf8
Copy-Item -LiteralPath $preferencesPath -Destination (Join-Path $artifactDir 'preferences.ini') -Force
if (Test-Path -LiteralPath $crtLogPath -PathType Leaf) {
    Copy-Item -LiteralPath $crtLogPath -Destination (Join-Path $artifactDir 'eMule CRT Debug Log.log') -Force
}

$summary = [ordered]@{
    helper = 'helper-runtime-pipe-live-session.ps1'
    build_exe_path = $buildExePath
    launched_exe_path = $launchedExePath
    artifact_dir = $artifactDir
    artifact_latest_dir = $artifactLatestDir
    seed_root = $seedRoot
    profile_base_root = $profileBaseRoot
    profile_runs_root = $profileRunsRoot
    profile_root = $profileRoot
    profile_latest_root = $profileLatestRoot
    profile_latest_published = $profileLatestPublished
    profile_latest_error = $profileLatestError
    config_dir = $configDir
    log_dir = $logDir
    scenario_profile = $ScenarioProfile
    matrix_repeat_count = $MatrixRepeatCount
    strict_matrix = [bool]$StrictMatrix
    launch_only = [bool]$LaunchOnly
    keep_running = [bool]$KeepRunning
    pipe_name = '\\.\pipe\emule-api'
    base_uri = $baseUri
    remote_token = $RemoteToken
    emule_process_id = $emuleProcess.Id
    remote_process_id = if ($null -ne $remoteHandle) { $remoteHandle.Process.Id } else { $null }
    started_processes = @($startedProcesses.ToArray())
    started_process_ids = @($startedProcesses | ForEach-Object { [int]$_.process_id })
    manifest_paths = @($manifestPaths)
    remote_health = $health
    server_connect = $serversStatus
    kad_connect = $kadStatus
    matrix_result = $matrixResult
    matrix_summary = $matrixSummary
    soak_result = $soakResult
    soak_summary = $soakSummary
    search_cycles = @($searchCycles)
    selected_download_link = $selectedDownloadLink
    synthetic_links = @($syntheticLinks)
    transfer_add_result = $transferAddResult
    transfer_churn_cycles = @($transferChurnCycles)
    freeze_reason = $freezeReason
    dump_path = $dumpPath
    sample_count = $runtimeSamples.Count
    session_failure = $sessionFailure
    session_failure_detail = $sessionFailureDetail
    cleanup_requested = (-not [bool]$KeepRunning)
    cleanup_attempts = $cleanupResult.attempts
    cleanup_success = [bool]$cleanupResult.success
    cleanup_stopped_process_ids = @($cleanupResult.stopped_process_ids)
    leftover_process_ids = @($cleanupResult.leftover_process_ids)
    leftover_processes = @($cleanupResult.leftover_processes)
    cleanup_error = $cleanupResult.error
    finished_at = (Get-Date).ToString('o')
}
$latestArtifactPublished = $false
$latestArtifactError = $null
if (-not $KeepRunning) {
    try {
        Publish-DirectorySnapshot -SourceDirectory $artifactDir -DestinationDirectory $artifactLatestDir
        $latestArtifactPublished = $true
    } catch {
        $latestArtifactError = $_.Exception.Message
    }
}
$summary.latest_artifact_published = $latestArtifactPublished
$summary.latest_artifact_error = $latestArtifactError
foreach ($entry in $summary.GetEnumerator()) {
    $manifest[$entry.Key] = $entry.Value
}
Write-SessionManifest -Manifest $manifest -Paths $manifestPaths
@(
    "Pipe live session"
    "build_exe_path: $buildExePath"
    "launched_exe_path: $launchedExePath"
    "artifact_dir: $artifactDir"
    "artifact_latest_dir: $artifactLatestDir"
    "profile_root: $profileRoot"
    "profile_latest_root: $profileLatestRoot"
    "scenario_profile: $ScenarioProfile"
    "matrix_failed: $($matrixSummary.failed)"
    "soak_failed: $($soakSummary.failed)"
    "search_query: $SearchQuery"
    "search_cycle_count: $SearchCycleCount"
    "transfer_churn_cycles: $TransferChurnCycles"
    "transfers_per_churn_cycle: $TransfersPerChurnCycle"
    "selected_download_link: $selectedDownloadLink"
    "freeze_reason: $freezeReason"
    "dump_path: $dumpPath"
    "sample_count: $($runtimeSamples.Count)"
    "cleanup_success: $([bool]$cleanupResult.success)"
    "leftover_process_ids: $(@($cleanupResult.leftover_process_ids) -join ',')"
    "cleanup_error: $($cleanupResult.error)"
    "session_failure: $sessionFailure"
) | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Output "Pipe live session artifact directory: $artifactDir"
if (-not [string]::IsNullOrWhiteSpace($sessionFailure)) {
    <#*
     * @brief Surface the recorded failure after artifacts are flushed so automation can fail fast without losing logs.
     #>
    Write-Error $sessionFailure
    exit 1
}

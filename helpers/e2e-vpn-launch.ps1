<#
.SYNOPSIS
Runs a clean end-to-end debug launch against the repo eMule build.

.DESCRIPTION
This helper resets LocalAppData state, downloads fresh emule-security bootstrap
artifacts, binds the session to a caller-provided VPN IPv4 address, enables
maximum disk logging, recursively shares a target directory tree, starts the
repo debug executable, and monitors process plus log activity for a bounded
time window.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$ShareRoot = 'C:\tmp\videodupez',

    [Parameter(Mandatory = $false)]
    [string]$BindAddress = '10.54.218.144',

    [Parameter(Mandatory = $false)]
    [int]$MonitorSec = 240,

    [Parameter(Mandatory = $false)]
    [int]$PollSec = 15,

    [Parameter(Mandatory = $false)]
    [string]$NodesDatUrl = 'http://upd.emule-security.org/nodes.dat',

    [Parameter(Mandatory = $false)]
    [string]$ServerMetUrl = 'http://upd.emule-security.org/server.met',

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

function Stop-ExistingEmuleProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExePath
    )

    $normalizedExePath = Get-NormalizedPath -Path $ExePath
    $processes = @(Get-Process -Name emule -ErrorAction SilentlyContinue)
    foreach ($process in $processes) {
        try {
            if ([string]::Equals((Get-NormalizedPath -Path $process.Path), $normalizedExePath, [System.StringComparison]::OrdinalIgnoreCase)) {
                Stop-Process -Id $process.Id -Force
            }
        } catch {
        }
    }
}

function Download-File {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,

        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    Invoke-WebRequest -Uri $Url -OutFile $DestinationPath
    $item = Get-Item -LiteralPath $DestinationPath
    if ($item.Length -le 0) {
        throw "Downloaded file '$DestinationPath' from '$Url' is empty."
    }
    return $item
}

function Write-RecursiveSharedDirs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ShareRoot,

        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    $shareRootWithSlash = Get-FolderPath -Path $ShareRoot
    $directories = New-Object System.Collections.Generic.List[string]
    $directories.Add($shareRootWithSlash)
    Get-ChildItem -LiteralPath $shareRootWithSlash -Directory -Recurse | Sort-Object FullName | ForEach-Object {
        $directories.Add((Get-FolderPath -Path $_.FullName))
    }
    Set-Content -LiteralPath $DestinationPath -Value $directories -Encoding Unicode
    return $directories.Count
}

function Write-LaunchPreferences {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PreferencesPath,

        [Parameter(Mandatory = $true)]
        [string]$IncomingDir,

        [Parameter(Mandatory = $true)]
        [string]$TempDir,

        [Parameter(Mandatory = $true)]
        [string]$BindAddress,

        [Parameter(Mandatory = $true)]
        [string]$NodesDatUrl
    )

    $content = @"
[eMule]
AppVersion=0.72a.1 x64 DEBUG
CreateCrashDump=2
Nick=CodexE2E
IncomingDir=$IncomingDir
TempDir=$TempDir
TempDirs=
Port=27198
UDPPort=27208
BindInterface=
BindInterfaceName=
BindAddr=$BindAddress
RandomizePortsOnStartup=0
ServerUDPPort=65535
Reconnect=1
Autoconnect=1
NetworkKademlia=1
NetworkED2K=1
Serverlist=1
AutoConnectStaticOnly=0
NodesDatUpdateUrl=$NodesDatUrl
VerboseOptions=1
Verbose=1
FullVerbose=1
DebugSourceExchange=1
LogBannedClients=1
LogRatingDescReceived=1
LogSecureIdent=1
LogFilteredIPs=1
LogFileSaving=1
LogA4AF=1
LogUlDlEvents=1
DebugServerTCP=1
DebugServerUDP=1
DebugServerSources=1
DebugServerSearches=1
DebugClientTCP=1
DebugClientUDP=1
DebugClientKadUDP=1
SaveLogToDisk=1
SaveDebugToDisk=1
ShowOverhead=1
CheckDiskspace=0
AutoRescanSharedFolders=1
AutoRescanSharedFoldersIntervalSec=600
AutoShareNewSharedSubdirs=1
ResolveSharedShellLinks=0
"@
    Set-Content -LiteralPath $PreferencesPath -Value $content -Encoding ascii
}

function Get-ProcessSocketsSummary {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId
    )

    $tcp = @(Get-NetTCPConnection -OwningProcess $ProcessId -ErrorAction SilentlyContinue)
    $udp = @(Get-NetUDPEndpoint -OwningProcess $ProcessId -ErrorAction SilentlyContinue)
    return [ordered]@{
        tcp = $tcp | Select-Object State,LocalAddress,LocalPort,RemoteAddress,RemotePort
        udp = $udp | Select-Object LocalAddress,LocalPort
    }
}

function Get-LogTail {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogDir
    )

    $tails = [ordered]@{}
    $logFiles = @(Get-ChildItem -LiteralPath $LogDir -Filter *.log -File -ErrorAction SilentlyContinue)
    foreach ($logFile in $logFiles) {
        $tails[$logFile.Name] = @(Get-Content -LiteralPath $logFile.FullName -Tail 10 -ErrorAction SilentlyContinue | ForEach-Object { $_.ToString() })
    }
    return $tails
}

$helperDir = Split-Path -Parent $PSCommandPath
$repoRoot = Get-NormalizedPath -Path (Join-Path $helperDir '..')
$workspaceRoot = Get-NormalizedPath -Path (Join-Path $repoRoot '..')
$buildScriptPath = Join-Path $workspaceRoot '23-build-emule-debug-incremental.cmd'
$exePath = Join-Path $repoRoot 'srchybrid\x64\Debug\emule.exe'
$stateRoot = Join-Path $env:LOCALAPPDATA 'eMule'
$configDir = Join-Path $stateRoot 'config'
$logDir = Join-Path $stateRoot 'logs'
$tempDir = Join-Path $stateRoot 'temp'
$artifactDir = Join-Path (Join-Path $workspaceRoot 'logs') ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-e2e-vpn-launch')
$manifestPath = Join-Path $artifactDir 'session-manifest.json'
$summaryPath = Join-Path $artifactDir 'session-summary.txt'

$shareRoot = Get-FolderPath -Path $ShareRoot
Ensure-Directory -Path $artifactDir

if (-not (Test-Path -LiteralPath $shareRoot -PathType Container)) {
    throw "Share root '$shareRoot' does not exist."
}

$bindCandidate = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -eq $BindAddress }
if ($null -eq $bindCandidate) {
    throw "Bind address '$BindAddress' is not assigned on this machine."
}

if (-not $SkipBuild) {
    & $buildScriptPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

Stop-ExistingEmuleProcess -ExePath $exePath

Ensure-Directory -Path $configDir
Ensure-Directory -Path $logDir
Ensure-Directory -Path $tempDir
Clear-DirectoryContents -Path $stateRoot
Ensure-Directory -Path $configDir
Ensure-Directory -Path $logDir
Ensure-Directory -Path $tempDir

$nodesPath = Join-Path $configDir 'nodes.dat'
$serverMetPath = Join-Path $configDir 'server.met'
$addressesPath = Join-Path $configDir 'addresses.dat'
$serverUrlsPath = Join-Path $configDir 'AC_ServerMetURLs.dat'
$sharedDirsPath = Join-Path $configDir 'shareddir.dat'
$preferencesPath = Join-Path $configDir 'preferences.ini'

$nodesInfo = Download-File -Url $NodesDatUrl -DestinationPath $nodesPath
$serverInfo = Download-File -Url $ServerMetUrl -DestinationPath $serverMetPath
Set-Content -LiteralPath $addressesPath -Value $ServerMetUrl -Encoding ascii
Set-Content -LiteralPath $serverUrlsPath -Value $ServerMetUrl -Encoding ascii
$sharedDirCount = Write-RecursiveSharedDirs -ShareRoot $shareRoot -DestinationPath $sharedDirsPath
Write-LaunchPreferences -PreferencesPath $preferencesPath -IncomingDir $shareRoot -TempDir (Get-FolderPath -Path $tempDir) -BindAddress $BindAddress -NodesDatUrl $NodesDatUrl

$manifest = [ordered]@{
    helper = 'e2e-vpn-launch.ps1'
    started_at = (Get-Date).ToString('o')
    exe_path = $exePath
    bind_address = $BindAddress
    bind_interface = $bindCandidate.InterfaceAlias
    share_root = $shareRoot
    shareddir_entries = $sharedDirCount
    state_root = $stateRoot
    config_dir = $configDir
    log_dir = $logDir
    nodes_url = $NodesDatUrl
    nodes_path = $nodesPath
    nodes_size = $nodesInfo.Length
    server_met_url = $ServerMetUrl
    server_met_path = $serverMetPath
    server_met_size = $serverInfo.Length
    artifact_dir = $artifactDir
    monitor_sec = $MonitorSec
    poll_sec = $PollSec
    launch_status = 'starting'
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$process = Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath) -PassThru
$manifest.process_id = $process.Id
$manifest.launch_status = 'running'
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8

$deadline = (Get-Date).AddSeconds($MonitorSec)
$monitorSnapshots = New-Object System.Collections.Generic.List[object]

while ((Get-Date) -lt $deadline) {
    $running = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if ($null -eq $running) {
        break
    }

    $socketSummary = Get-ProcessSocketsSummary -ProcessId $process.Id
    $logTail = Get-LogTail -LogDir $logDir
    $snapshot = [ordered]@{
        at = (Get-Date).ToString('o')
        process = [ordered]@{
            id = $running.Id
            cpu = $running.CPU
            working_set = $running.WorkingSet64
            responding = $running.Responding
        }
        sockets = $socketSummary
        log_tail = $logTail
    }
    $monitorSnapshots.Add([pscustomobject]$snapshot)
    Start-Sleep -Seconds $PollSec
}

$running = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
if ($null -ne $running -and -not $KeepRunning) {
    Stop-Process -Id $process.Id -Force
    Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
}

$manifest.finished_at = (Get-Date).ToString('o')
$manifest.launch_status = if ($null -ne (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) { 'running' } else { 'stopped' }
$manifest.log_files = @(Get-ChildItem -LiteralPath $logDir -Filter *.log -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
$manifest.monitor_snapshots = $monitorSnapshots
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8

if (Test-Path -LiteralPath $configDir) {
    Copy-Item -LiteralPath $configDir -Destination (Join-Path $artifactDir 'config') -Recurse -Force
}
if (Test-Path -LiteralPath $logDir) {
    Copy-Item -LiteralPath $logDir -Destination (Join-Path $artifactDir 'logs') -Recurse -Force
}

$summary = @(
    "eMule VPN e2e launch"
    "bind_address=$BindAddress"
    "bind_interface=$($bindCandidate.InterfaceAlias)"
    "share_root=$shareRoot"
    "shareddir_entries=$sharedDirCount"
    "nodes_size=$($nodesInfo.Length)"
    "server_met_size=$($serverInfo.Length)"
    "process_id=$($process.Id)"
    "final_status=$($manifest.launch_status)"
    "artifact_dir=$artifactDir"
)
Set-Content -LiteralPath $summaryPath -Value $summary -Encoding utf8

Write-Host "Artifacts written to: $artifactDir"
Write-Host "Process id: $($process.Id)"
Write-Host "Final status: $($manifest.launch_status)"

#Requires -Version 5.1
#Requires -RunAsAdministrator

<#
.SYNOPSIS
Creates, updates, or removes the Windows Defender Firewall rule for eMule.

.DESCRIPTION
This script must remain compatible with Windows built-in PowerShell.exe
(Windows PowerShell 5.1) on Windows 10 and Windows 11.

When -ExePath is not supplied, the script searches the parent workspace
directory for emule.exe and uses the best matching build output as the
default. If no executable is found, the script prompts for a path.
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $false)]
    [string]$ExePath,

    [Parameter(Mandatory = $false)]
    [string]$RuleName = 'eMule',

    [switch]$Remove
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS
Returns the normalized full path for an existing file.

.PARAMETER Path
Path to validate and normalize.
#>
function Get-NormalizedExistingLeafPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'Path must not be empty.'
    }

    $resolvedPath = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $fullPath = [System.IO.Path]::GetFullPath($resolvedPath.ProviderPath)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Executable path '$fullPath' does not exist."
    }

    return $fullPath
}

<#
.SYNOPSIS
Returns the repository root relative to the scripts directory.
#>
function Get-WorkspaceRoot {
    $workspaceRoot = Join-Path $PSScriptRoot '..'
    return [System.IO.Path]::GetFullPath($workspaceRoot)
}

<#
.SYNOPSIS
Finds the preferred emule.exe under the workspace root.

.PARAMETER WorkspaceRoot
Repository root used for recursive search.
#>
function Get-DefaultEmuleExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    $candidatePaths = @(
        Get-ChildItem -LiteralPath $WorkspaceRoot -Filter 'emule.exe' -File -Recurse -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName
    ) | ForEach-Object {
        [System.IO.Path]::GetFullPath($_)
    } | Sort-Object -Unique

    if ($candidatePaths.Count -eq 0) {
        return $null
    }

    # Prefer the normal debug build output first, then release, before falling back
    # to any other deterministic match under the workspace root.
    $preferredPaths = @(
        [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot 'srchybrid\x64\Debug\emule.exe')),
        [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot 'srchybrid\x64\Release\emule.exe'))
    )

    foreach ($preferredPath in $preferredPaths) {
        if ($candidatePaths -contains $preferredPath) {
            return $preferredPath
        }
    }

    return ($candidatePaths | Sort-Object | Select-Object -First 1)
}

<#
.SYNOPSIS
Reads the program paths associated with existing firewall rules.

.PARAMETER Rules
Firewall rules to inspect.
#>
function Get-RulePrograms {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Rules
    )

    $programs = @()
    foreach ($rule in $Rules) {
        $appFilters = Get-NetFirewallApplicationFilter -AssociatedNetFirewallRule $rule -ErrorAction SilentlyContinue
        foreach ($filter in $appFilters) {
            if (-not [string]::IsNullOrWhiteSpace($filter.Program)) {
                $programs += $filter.Program
            }
        }
    }

    return $programs
}

<#
.SYNOPSIS
Resolves the executable path from parameter, search result, or prompt.
#>
function Resolve-EmuleExecutablePath {
    param(
        [Parameter(Mandatory = $false)]
        [string]$CandidatePath
    )

    if (-not [string]::IsNullOrWhiteSpace($CandidatePath)) {
        return Get-NormalizedExistingLeafPath -Path $CandidatePath
    }

    $workspaceRoot = Get-WorkspaceRoot
    $defaultExePath = Get-DefaultEmuleExecutable -WorkspaceRoot $workspaceRoot
    if (-not [string]::IsNullOrWhiteSpace($defaultExePath)) {
        Write-Host "Using detected eMule executable '$defaultExePath'."
        return $defaultExePath
    }

    $promptedPath = Read-Host 'Enter the full path to emule.exe'
    if ([string]::IsNullOrWhiteSpace($promptedPath)) {
        throw 'No executable path was provided.'
    }

    return Get-NormalizedExistingLeafPath -Path $promptedPath
}

try {
    if ($Remove) {
        $existingRules = @(Get-NetFirewallRule -DisplayName $RuleName -ErrorAction SilentlyContinue)
        if ($existingRules.Count -eq 0) {
            Write-Host "No firewall rule named '$RuleName' exists."
            exit 0
        }

        if ($PSCmdlet.ShouldProcess($RuleName, 'Remove Windows Firewall rule')) {
            $existingRules | Remove-NetFirewallRule
            Write-Host "Removed firewall rule '$RuleName'."
        }

        exit 0
    }

    $resolvedExePath = Resolve-EmuleExecutablePath -CandidatePath $ExePath

    $existingRules = @(Get-NetFirewallRule -DisplayName $RuleName -ErrorAction SilentlyContinue)
    $existingPrograms = @()
    $firewallRuleChanged = $false
    if ($existingRules.Count -gt 0) {
        $existingPrograms = @(Get-RulePrograms -Rules $existingRules)
        if ($PSCmdlet.ShouldProcess($RuleName, 'Replace existing Windows Firewall rule')) {
            $existingRules | Remove-NetFirewallRule
        }
    }

    if ($PSCmdlet.ShouldProcess($resolvedExePath, 'Create Windows Firewall allow rule for eMule')) {
        New-NetFirewallRule `
            -DisplayName $RuleName `
            -Direction Inbound `
            -Action Allow `
            -Enabled True `
            -Profile Any `
            -Program $resolvedExePath | Out-Null
        $firewallRuleChanged = $true
    }

    if ($firewallRuleChanged) {
        if ($existingPrograms.Count -eq 0) {
            Write-Host "Created firewall rule '$RuleName' for '$resolvedExePath'."
        } else {
            Write-Host "Replaced firewall rule '$RuleName' for '$resolvedExePath'."
            Write-Host "Previous rule program paths: $($existingPrograms -join ', ')"
        }
    }
} catch {
    Write-Error $_
    exit 1
}

#Requires -RunAsAdministrator

<#
.SYNOPSIS
Creates, updates, or removes the Windows Defender Firewall rule for eMule.

.DESCRIPTION
This helper is intended for Windows 10 and Windows 11. It manages one inbound
allow rule for the selected emule.exe path and applies it to all firewall
profiles. The rule is application-based rather than port-based, so it replaces
the old XP-era firewall-opening model used by legacy eMule code.
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

function Get-NormalizedFullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).ProviderPath)
}

function Get-RulePrograms {
    param(
        [Parameter(Mandatory = $true)]
        $Rules
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

try {
    if ($Remove) {
        $existingRules = @(Get-NetFirewallRule -DisplayName $RuleName -ErrorAction SilentlyContinue)
        if ($existingRules.Count -eq 0) {
            Write-Host "No firewall rule named '$RuleName' exists."
            exit 0
        }

        if ($PSCmdlet.ShouldProcess($RuleName, 'Remove Windows Firewall rule')) {
            $existingRules | Remove-NetFirewallRule
        }
        Write-Host "Removed firewall rule '$RuleName'."
        exit 0
    }

    if ([string]::IsNullOrWhiteSpace($ExePath)) {
        throw "The -ExePath parameter is required unless -Remove is used."
    }

    $resolvedExePath = Get-NormalizedFullPath -Path $ExePath
    if (-not (Test-Path -LiteralPath $resolvedExePath -PathType Leaf)) {
        throw "Executable path '$resolvedExePath' does not exist."
    }

    $existingRules = @(Get-NetFirewallRule -DisplayName $RuleName -ErrorAction SilentlyContinue)
    $existingPrograms = @()
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
    }

    if ($existingPrograms.Count -eq 0) {
        Write-Host "Created firewall rule '$RuleName' for '$resolvedExePath'."
    } else {
        Write-Host "Replaced firewall rule '$RuleName' for '$resolvedExePath'."
        Write-Host "Previous rule program paths: $($existingPrograms -join ', ')"
    }
} catch {
    Write-Error $_
    exit 1
}

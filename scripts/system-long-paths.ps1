#Requires -Version 5.1
#Requires -RunAsAdministrator

<#
.SYNOPSIS
Enables Win32 long path support for the local machine.

.DESCRIPTION
This script must remain compatible with Windows built-in PowerShell.exe
(Windows PowerShell 5.1) on Windows 10 and Windows 11.

It enables HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled
and leaves the machine unchanged when the setting is already enabled.
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

try {
    $regPath = 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'
    $regName = 'LongPathsEnabled'

    $currentValue = (Get-ItemProperty -Path $regPath -Name $regName -ErrorAction SilentlyContinue).$regName
    if ($currentValue -eq 1) {
        Write-Host 'Long paths are already enabled.'
        exit 0
    }

    # The registry write is the only state-changing step; ShouldProcess keeps
    # the script safe for -WhatIf use in Windows PowerShell.
    if ($PSCmdlet.ShouldProcess("$regPath\$regName", 'Enable Win32 long path support')) {
        Set-ItemProperty -Path $regPath -Name $regName -Value 1 -Type DWord
        Write-Host 'Long paths enabled. A reboot may be required for all applications to pick this up.'
    }
} catch {
    Write-Error $_
    exit 1
}

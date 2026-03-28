#Requires -RunAsAdministrator

$regPath = 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'
$regName = 'LongPathsEnabled'

$current = (Get-ItemProperty -Path $regPath -Name $regName -ErrorAction SilentlyContinue).$regName

if ($current -eq 1) {
    Write-Host "Long paths already enabled."
} else {
    Set-ItemProperty -Path $regPath -Name $regName -Value 1 -Type DWord
    Write-Host "Long paths enabled. A reboot may be required for all applications to pick this up."
}

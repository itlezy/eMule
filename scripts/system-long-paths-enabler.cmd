@echo off
setlocal
REM Launch the sibling PowerShell script explicitly with Windows built-in PowerShell.exe.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0system-long-paths-enabler.ps1" %*
set "exit_code=%ERRORLEVEL%"
endlocal & exit /b %exit_code%

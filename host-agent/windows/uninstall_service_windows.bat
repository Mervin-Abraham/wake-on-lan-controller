@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SERVICE_NAME=WolShutdownSleep"
set "FIREWALL_RULE=PC Shutdown Listener"

sc.exe stop "%SERVICE_NAME%" >nul 2>&1
sc.exe delete "%SERVICE_NAME%" >nul 2>&1
netsh advfirewall firewall delete rule name="%FIREWALL_RULE%" >nul 2>&1

echo Service removed. Files and venv were kept.
echo.
echo Uninstallation complete! Press any key to close this window.
pause >nul



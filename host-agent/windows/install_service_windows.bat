@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Always run from this script's folder
cd /d %~dp0

REM Resolve repo root (host-agent/windows -> repo root is ..\.. )
for %%I in ("%~dp0..\..") do set "ROOT_DIR=%%~fI"
set "VENV_DIR=%ROOT_DIR%\.venv"
set "PY_EXE=%VENV_DIR%\Scripts\python.exe"
set "LISTENER=%ROOT_DIR%\host-agent\listener.py"
set "WRAPPER=%ROOT_DIR%\host-agent\windows\run_service.bat"
set "REQ=%ROOT_DIR%\requirements.txt"
set "SERVICE_NAME=WolShutdownSleep"
set "SERVICE_DESC=PC Shutdown & Sleep Listener (WoL Companion)"
set "FIREWALL_RULE=PC Shutdown Listener"

echo Repo root: %ROOT_DIR%

echo [1/3] Creating Python virtual environment (if missing)...
if not exist "%VENV_DIR%" (
    python -m venv "%VENV_DIR%"
    if %errorlevel% neq 0 (
        echo Error: Failed to create virtual environment.
        exit /b 1
    )
)

echo [2/3] Installing dependencies...
echo Checking requirements file: %REQ%
if not exist "%REQ%" (
    echo Error: Requirements file not found: %REQ%
    exit /b 1
)

echo Upgrading pip...
"%PY_EXE%" -m pip install --upgrade pip
if %errorlevel% neq 0 (
    echo Error: Failed to upgrade pip.
    exit /b 1
)

echo Installing requirements...
"%PY_EXE%" -m pip install -r "%REQ%"
if %errorlevel% neq 0 (
    echo Error: Failed to install dependencies from %REQ%
    echo Please check the requirements file and try again.
    exit /b 1
)

echo [3/3] Creating Windows service (auto-start before sign-in)...
set "BIN_PATH=\"%WRAPPER%\""
sc.exe create "%SERVICE_NAME%" binPath= "%BIN_PATH%" start= auto
sc.exe description "%SERVICE_NAME%" "%SERVICE_DESC%"
sc.exe config "%SERVICE_NAME%" obj= LocalSystem
sc.exe config "%SERVICE_NAME%" start= auto

REM Open firewall port 8888 once (idempotent)
netsh advfirewall firewall show rule name="%FIREWALL_RULE%" >nul 2>&1
if errorlevel 1 (
    netsh advfirewall firewall add rule name="%FIREWALL_RULE%" dir=in action=allow protocol=TCP localport=8888 >nul
)

sc.exe start "%SERVICE_NAME%" >nul
echo Service "%SERVICE_NAME%" installed and started.
echo Check: sc qc %SERVICE_NAME%
echo.
echo Installation complete! Press any key to close this window.
pause >nul



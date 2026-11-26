@echo off
setlocal
title RA6E2 SONAR-3D — 3D Ultrasonic Point Cloud Scanner Dashboard

echo ======================================================================
echo   Renesas BGK-RA6E2 SONAR-3D Ultrasonic Point Cloud Scanner
echo   Host Desktop Dashboard Launcher
echo ======================================================================

set "PYTHONPATH=%~dp0"
cd /d "%~dp0"

python -c "import PySide6, pyqtgraph, OpenGL, serial, numpy" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Missing Python dependencies. Installing required packages...
    pip install PySide6 pyqtgraph PyOpenGL pyserial numpy scipy
)

echo Starting RA6E2 SONAR-3D Dashboard...
python "%~dp0dashboard\main.py"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Application exited with error code %ERRORLEVEL%.
    pause
)

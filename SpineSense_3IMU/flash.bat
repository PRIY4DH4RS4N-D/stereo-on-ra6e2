@echo off
setlocal
echo ======================================================================
echo   Flashing SpineSense_3IMU Firmware to Renesas BGK-RA6E2 via J-Link
echo ======================================================================

set "JLINK_EXE=C:\Program Files\SEGGER\JLink_V976a\JLink.exe"
if not exist "%JLINK_EXE%" (
    echo [ERROR] JLink.exe not found at %JLINK_EXE%
    pause
    exit /b 1
)

"%JLINK_EXE%" -device R7FA6E2BB -if SWD -speed 4000 -autoconnect 1 -CommanderScript "%~dp0flash.jlink"
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] SpineSense_3IMU firmware flashed and target running!
) else (
    echo [ERROR] Flashing failed!
)
pause

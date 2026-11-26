@echo off
set JLINK_EXE="C:\Program Files\SEGGER\JLink_V976a\JLink.exe"
if not exist %JLINK_EXE% (
    for /d %%d in ("C:\Program Files\SEGGER\JLink*") do (
        if exist "%%d\JLink.exe" set JLINK_EXE="%%d\JLink.exe"
    )
)
echo ========================================================
echo  Flashing RA6E2 Ultrasonic Project via Onboard J-Link
echo ========================================================
%JLINK_EXE% -CommanderScript "%~dp0flash.jlink"
echo ========================================================
echo  Flash complete! View serial output in PuTTY or RTT Viewer.
echo ========================================================
pause

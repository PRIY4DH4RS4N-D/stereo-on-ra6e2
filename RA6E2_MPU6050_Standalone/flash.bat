@echo off
echo ===================================================
echo   Flashing RA6E2 DUAL MPU6050 Firmware...
echo ===================================================
"C:\Program Files\SEGGER\JLink_V976a\JLink.exe" -Device R7FA6E2BB -If SWD -Speed 4000 -Autoconnect 1 -CommandFile "%~dp0flash_run.jlink"
echo.
echo ===================================================
echo   Flashing Complete!
echo   Open PuTTY on COM20 at 115200 baud.
echo   Or run read_serial.bat to view live stream.
echo ===================================================
pause

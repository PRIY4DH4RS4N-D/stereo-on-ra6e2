@echo off
echo ========================================================
echo Flashing SD Card Test to Renesas BGK-RA6E2 via J-Link...
echo ========================================================
"C:\Program Files\SEGGER\JLink_V976a\JLink.exe" -CommanderScript "%~dp0flash.jlink"
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo Flash SUCCESS! Firmware is now running on the board.
    echo Open COM20 in PuTTY at 115200 baud to view logs.
    echo ========================================================
) else (
    echo.
    echo [ERROR] Flashing failed! Check USB cable connection.
)
pause

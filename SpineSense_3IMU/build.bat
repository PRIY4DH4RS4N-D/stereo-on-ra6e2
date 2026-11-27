@echo off
setlocal
echo ======================================================================
echo   Building SpineSense_3IMU MPU6050 Firmware (GCC Arm Embedded)
echo ======================================================================

set "PATH=C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin;C:\Users\Priyadarsan D\.eclipse\com.renesas.platform_1435879475\Utilities;%PATH%"

cd /d "%~dp0Debug"
make -j8 clean
make -j8 all

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ======================================================================
    echo   [SUCCESS] Build completed with 0 errors!
    echo   Outputs:
    echo     - SpineSense_3IMU.elf
    echo     - SpineSense_3IMU.hex
    echo     - SpineSense_3IMU.srec
    echo ======================================================================
) else (
    echo.
    echo [ERROR] Build failed! Check compiler output above.
)
pause

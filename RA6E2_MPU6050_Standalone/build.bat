@echo off
setlocal
echo ======================================================================
echo   Building RA6E2 MPU6050 Standalone Firmware (GCC Arm Embedded)
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
    echo     - RA6E2_MPU6050_Standalone.elf
    echo     - RA6E2_MPU6050_Standalone.hex
    echo     - RA6E2_MPU6050_Standalone.srec
    echo ======================================================================
) else (
    echo.
    echo [ERROR] Build failed! Check compiler output above.
)
pause

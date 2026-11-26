@echo off
set "PATH=C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin;C:\Users\Priyadarsan D\.eclipse\com.renesas.platform_1435879475\Utilities;%PATH%"
echo Building RA6E2 SONAR-3D Firmware...
make -C "%~dp0Debug" -j8 all
if %ERRORLEVEL% NEQ 0 (
    echo [BUILD ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)
echo [BUILD SUCCESS] RA6E2_SONAR_3D.elf and RA6E2_SONAR_3D.hex are ready!

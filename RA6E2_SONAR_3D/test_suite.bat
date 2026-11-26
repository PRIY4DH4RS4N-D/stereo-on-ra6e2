@echo off
setlocal
title RA6E2 SONAR-3D — Test Suite Runner

set "PYTHONPATH=%~dp0"
cd /d "%~dp0"

echo Running RA6E2 SONAR-3D Unit Tests...
python -m pytest "%~dp0dashboard\tests" -v

pause

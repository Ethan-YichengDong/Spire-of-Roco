@echo off
setlocal

cd /d "%~dp0"
chcp 65001 >nul

if not exist "third_party\easyx4mingw\include\graphics.h" goto setup_easyx
if not exist "third_party\easyx4mingw\lib64\libeasyx.a" goto setup_easyx
goto build_and_run

:setup_easyx
    echo EasyX for MinGW is missing. Downloading project-local dependency...
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\setup_easyx_mingw.ps1
    if errorlevel 1 exit /b 1

:build_and_run
call build_gui_mingw.bat %*
exit /b %ERRORLEVEL%

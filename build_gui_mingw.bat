@echo off
setlocal
chcp 65001 >nul

set OUT=roco_gui.exe
set EASYX_DIR=third_party\easyx4mingw
set UTF8_FLAGS=-finput-charset=UTF-8 -fexec-charset=UTF-8

if not exist "%EASYX_DIR%\include\graphics.h" (
    echo EasyX for MinGW is not installed locally.
    echo Run: powershell -ExecutionPolicy Bypass -File tools\setup_easyx_mingw.ps1
    exit /b 1
)

echo Cleaning previous GUI build outputs...
del /Q *.obj *.o %OUT% 2>nul

where gcc >nul 2>nul
if errorlevel 1 (
    echo gcc was not found. Install MinGW-w64 or add it to PATH.
    exit /b 1
)

where g++ >nul 2>nul
if errorlevel 1 (
    echo g++ was not found. Install MinGW-w64 or add it to PATH.
    exit /b 1
)

echo Compiling GUI build with MinGW-w64 and project-local EasyX...
gcc -Wall -Wextra -DUSE_EASYX -c src_data\data_manager.c src_data\battle_calculator.c src_engine\main.c src_engine\game_engine.c src_ai\ai_bridge.c
if errorlevel 1 exit /b 1

g++ -Wall -Wextra %UTF8_FLAGS% -DUSE_EASYX -I"%EASYX_DIR%\include" -c src_gui\gui_manager.cpp
if errorlevel 1 exit /b 1

g++ data_manager.o battle_calculator.o main.o game_engine.o ai_bridge.o gui_manager.o -L"%EASYX_DIR%\lib64" -leasyx -lws2_32 -lgdi32 -lole32 -o %OUT%
if errorlevel 1 exit /b 1

if "%~1"=="--no-run" goto done
if "%ROCO_NO_RUN%"=="1" goto done

echo Build succeeded. Running %OUT%...
%OUT%
exit /b %ERRORLEVEL%

:done
echo Build succeeded. Output: %OUT%
exit /b 0

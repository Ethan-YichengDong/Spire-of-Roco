@echo off
setlocal

set OUT=windows_smoketest.exe
if "%ROCO_SMOKE_MAX_ROUNDS%"=="" set ROCO_SMOKE_MAX_ROUNDS=3

echo Cleaning previous Windows build outputs...
del /Q *.obj *.o %OUT% 2>nul

where cl >nul 2>nul
if %ERRORLEVEL% EQU 0 goto build_msvc

where gcc >nul 2>nul
if %ERRORLEVEL% EQU 0 goto build_mingw

echo No supported Windows C/C++ compiler found.
echo Install Visual Studio Build Tools or MinGW-w64, then run this script again.
exit /b 1

:build_msvc
echo Compiling smoke test with MSVC...
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /c /Tcsrc_data\data_manager.c /Fodata_manager.obj
if errorlevel 1 exit /b 1
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /c /Tcsrc_data\battle_calculator.c /Fobattle_calculator.obj
if errorlevel 1 exit /b 1
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /c /Tcsrc_engine\main.c /Fomain.obj
if errorlevel 1 exit /b 1
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /c /Tcsrc_engine\game_engine.c /Fogame_engine.obj
if errorlevel 1 exit /b 1
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /c /Tcsrc_ai\ai_bridge.c /Foai_bridge.obj
if errorlevel 1 exit /b 1
cl /nologo /W4 /EHsc /c /Tpsrc_smoketest\windows_cli_manager.cpp /Fowindows_cli_manager.obj
if errorlevel 1 exit /b 1
link /nologo data_manager.obj battle_calculator.obj main.obj game_engine.obj ai_bridge.obj windows_cli_manager.obj Ws2_32.lib /OUT:%OUT%
if errorlevel 1 exit /b 1
goto run_smoke

:build_mingw
echo Compiling smoke test with MinGW-w64...
gcc -Wall -Wextra -c src_data\data_manager.c src_data\battle_calculator.c src_engine\main.c src_engine\game_engine.c src_ai\ai_bridge.c
if errorlevel 1 exit /b 1
g++ -Wall -Wextra -c src_smoketest\windows_cli_manager.cpp
if errorlevel 1 exit /b 1
g++ data_manager.o battle_calculator.o main.o game_engine.o ai_bridge.o windows_cli_manager.o -lws2_32 -o %OUT%
if errorlevel 1 exit /b 1
goto run_smoke

:run_smoke
echo Compilation success. Running smoke test...
%OUT%
exit /b %ERRORLEVEL%

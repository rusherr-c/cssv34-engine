@echo off

:startup
if /i "%1"=="execute" (goto main)

:vcvars
call "C:\Program Files\Microsoft Visual Studio\16\Community\VC\Auxiliary\Build\vcvars32.bat" > nul 2>&1
call "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars32.bat" > nul 2>&1
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" > nul 2>&1
start cmd /c "%cd%\build execute"
exit /b

:main
if %errorlevel% NEQ 0 (echo VCVars is undefined)
cl.exe /LD steam_api.cpp > nul 2>&1
del /s /q *.obj > nul 2>&1
del /s /q *.exp > nul 2>&1
cls 
echo Publishing steam_api.lib to ..\..\lib\public
move steam_api.lib ..\..\lib\public >nul
echo Publishing steam_api.dll to ..\..\..\game\bin
move steam_api.dll ..\..\..\game\bin >nul
:fall
timeout /t 2 >nul
exit /b
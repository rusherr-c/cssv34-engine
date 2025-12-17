@echo off
cd %~dp0

cl.exe >nul 2>&1

if errorlevel 9009 (
echo You must run this in x86 Native Tools VS Command Prompt.
)

echo ---- COMPILING ----
cl /c steam_api.cpp 
lib /def:steam_api.def /out:steam_api.lib /machine:x86 steam_api.obj original_exports\steam_api.lib
echo ---- DONE COMPILING AT %TIME% ----
echo ---- CLEANING UP ----
del /s /q steam_api.obj >nul 2>&1
del /s /q steam_api.exp >nul 2>&1
echo ---- DONE ----
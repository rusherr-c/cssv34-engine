@echo off
setlocal

rem ================================
rem ==== MOD PATH CONFIGURATIONS ===


set SOURCEDIR=..\..
set GAMEDIR__=%cd%\..\..\..\game\mod_hl2
set SDKBINDIR__=%cd%\..\..\..\game\bin

rem ==== Convert paths to 8.3 format
FOR %%I IN (%GAMEDIR__%) DO SET GAMEDIR=%%~sI
FOR %%I IN (%SDKBINDIR__%) DO SET SDKBINDIR=%%~sI

rem ==== MOD PATH CONFIGURATIONS END ===
rem ====================================

echo SET 8.3 VARS
echo GAMEDIR: %gamedir%
echo SOURCEDIR: %sourcedir%
echo SDKBINDIR: %sdkbindir%

call buildsdkshaders.bat

@echo Finished building shaders
@pause

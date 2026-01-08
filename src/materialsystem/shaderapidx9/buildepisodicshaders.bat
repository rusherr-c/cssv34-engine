@echo off
setlocal

rem =================================
rem ==== MOD PATH CONFIGURATIONS ====

rem == Set the absolute path to your mod's game directory here ==
rem == Note that this path needs does not support long file/directory names ==
rem == So instead of a path such as "C:\Program Files\Steam\steamapps\mymod" ==
rem == you need to find the 8.3 abbreviation for the directory name using 'dir /x' ==
rem == and set the directory to something like C:\PROGRA~2\Steam\steamapps\sourcemods\mymod ==
set GAMEDIR=..\..\..\game\mod_hl2

rem == Set the relative path to SourceSDK\bin\orangebox\bin ==
rem == As above, this path does not support long directory names or spaces ==
rem == e.g. ..\..\..\..\..\PROGRA~2\Steam\steamapps\<USER NAME>\sourcesdk\bin\orangebox\bin ==
set SDKBINDIR=..\..\..\game\bin

rem ==  Set the Path to your mod's root source code ==
rem This should already be correct, accepts relative paths only!
set SOURCEDIR=..\..

rem ==== MOD PATH CONFIGURATIONS END ====
rem =====================================


call buildsdkshaders.bat

@echo Finished building shaders
@pause

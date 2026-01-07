@echo off
setlocal

:CleanOtherStuff
if exist debug_dx9 rd /s /q debug_dx9
if exist fxctmp9 rd /s /q fxctmp9
if exist pshtmp9 rd /s /q pshtmp9
if exist shaders rd /s /q shaders
if exist vshtmp9 rd /s /q vshtmp9
goto end


:end
exit/b
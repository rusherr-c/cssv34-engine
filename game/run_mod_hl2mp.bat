@echo off

set cmd_line=-console -sw -noborder -game mod_hl2mp

start "" hl2.exe %cmd_line%
echo hl2.exe %cmd_line%

timeout 3 >nul
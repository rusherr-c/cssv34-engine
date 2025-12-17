@echo off
set cmd_line=-console -sw -noborder -game mod_cstrike

start "" hl2.exe %cmd_line%
echo hl2.exe %cmd_line%

timeout 3 >nul
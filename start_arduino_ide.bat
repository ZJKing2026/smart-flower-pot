@echo off
title Arduino IDE Launcher
echo ============================================
echo  Arduino IDE Launcher (Fix Chinese Path Issue)
echo ============================================
echo.
echo LOCALAPPDATA: D:\tmp\localappdata
echo TMP:         D:\tmp
echo.
set "LOCALAPPDATA=D:\tmp\localappdata"
set "TMP=D:\tmp"
set "TEMP=D:\tmp"
start "" "D:\Arduino IDE\Arduino IDE.exe"
echo Arduino IDE started.
echo Close this window - Arduino IDE runs independently.
echo.
pause

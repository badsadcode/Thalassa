@echo off
title THALASSA - join
echo.
echo  THALASSA - joining a game
echo.
set /p HOSTADDRESS= Host's address (for example 192.168.1.20):
if "%HOSTADDRESS%"=="" (
    echo  No address entered.
    pause
    exit /b 1
)
start "" "%~dp0Thalassa.exe" %HOSTADDRESS% -windowed -ResX=1600 -ResY=900 -log

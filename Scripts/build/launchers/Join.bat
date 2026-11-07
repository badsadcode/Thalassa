@echo off
title THALASSA - join
echo.
echo  THALASSA - joining a game
echo.
echo  Type the host's address. On the same network that is like 192.168.1.20.
echo  Over a tunnel such as playit.gg, type the address and port it gives you,
echo  like example.playit.gg:12345.
echo.
set /p HOSTADDRESS= Host's address:
if "%HOSTADDRESS%"=="" (
    echo  No address entered.
    pause
    exit /b 1
)
start "" "%~dp0Thalassa.exe" %HOSTADDRESS% -windowed -ResX=1600 -ResY=900 -log

@echo off
title THALASSA - host
echo.
echo  THALASSA - hosting a game
echo.
echo  Tell the other player one of these addresses (the one on your home network,
echo  usually starting with 192.168. or 10.):
echo.
ipconfig | findstr /c:"IPv4"
echo.
echo  If Windows asks whether to allow THALASSA through the firewall, allow it on
echo  private networks, or the other player cannot connect.
echo.
start "" "%~dp0Thalassa.exe" "/Game/TestMaps/L_ShipMotion?listen" -windowed -ResX=1600 -ResY=900 -log

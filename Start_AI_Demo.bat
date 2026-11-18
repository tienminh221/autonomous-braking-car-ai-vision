@echo off
title Autonomous Car AI Vision Studio - Teachable Machine Demo
cd /d "%~dp0"
echo Dang khoi dong phan mem AI Vision & Teachable Machine Studio...

where msedge >nul 2>nul
if %errorlevel%==0 (
    start "" msedge --app="file:///%~dp0index.html" --window-size=1360,840
    exit
)

where chrome >nul 2>nul
if %errorlevel%==0 (
    start "" chrome --app="file:///%~dp0index.html" --window-size=1360,840
    exit
)

start "" "%~dp0index.html"

@echo off
REM Build standalone qrreader.exe with PyInstaller (Windows).
REM Double-click or run from cmd:
REM   scripts\build-pyinstaller.bat

setlocal EnableExtensions
cd /d "%~dp0\.."

where powershell >nul 2>&1
if errorlevel 1 (
  echo PowerShell is required to run scripts\build-pyinstaller.ps1
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-pyinstaller.ps1"
exit /b %ERRORLEVEL%

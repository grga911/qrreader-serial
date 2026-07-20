@echo off
REM Install qrreader.exe for the current Windows user (+ Startup shortcut).
REM   scripts\install-pyinstaller.bat
REM   scripts\install-pyinstaller.bat -ComPort COM3
REM   scripts\install-pyinstaller.bat -NoStartup

setlocal EnableExtensions
cd /d "%~dp0\.."

where powershell >nul 2>&1
if errorlevel 1 (
  echo PowerShell is required to run scripts\install-pyinstaller.ps1
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-pyinstaller.ps1" %*
exit /b %ERRORLEVEL%

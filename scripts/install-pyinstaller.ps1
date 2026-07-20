# Install qrreader.exe for the current Windows user and optionally add Startup.
# Prerequisites: build first with .\scripts\build-pyinstaller.ps1
#
#   .\scripts\install-pyinstaller.ps1
#   .\scripts\install-pyinstaller.ps1 -NoStartup
#   .\scripts\install-pyinstaller.ps1 -ComPort COM3

param(
    [string]$ComPort = "",
    [switch]$NoStartup
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $RootDir

$ExeSrc = Join-Path $RootDir "dist\qrreader.exe"
if (-not (Test-Path $ExeSrc)) {
    throw "Missing dist\qrreader.exe — run: .\scripts\build-pyinstaller.ps1"
}

$InstallDir = Join-Path $env:LOCALAPPDATA "qrreader"
$ExeDst = Join-Path $InstallDir "qrreader.exe"
$ConfigDir = Join-Path $env:APPDATA "qrreader"

Write-Host "==> Installing to $InstallDir"
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -Force $ExeSrc $ExeDst

$VersionSrc = Join-Path $RootDir "VERSION"
if (Test-Path $VersionSrc) {
    Copy-Item -Force $VersionSrc (Join-Path $InstallDir "VERSION")
}

New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
$PortFile = Join-Path $ConfigDir "port"
if ($ComPort) {
    Set-Content -Path $PortFile -Value $ComPort.Trim() -Encoding ascii
    Write-Host "==> Wrote COM port $ComPort to $PortFile"
} elseif (-not (Test-Path $PortFile)) {
    Set-Content -Path $PortFile -Value "COM21" -Encoding ascii
    Write-Host "==> Created default port file: $PortFile (COM21)"
}

if (-not $NoStartup) {
    $StartupDir = [Environment]::GetFolderPath("Startup")
    $ShortcutPath = Join-Path $StartupDir "qrreader.lnk"
    Write-Host "==> Creating Startup shortcut: $ShortcutPath"
    $Wsh = New-Object -ComObject WScript.Shell
    $Shortcut = $Wsh.CreateShortcut($ShortcutPath)
    $Shortcut.TargetPath = $ExeDst
    $Shortcut.WorkingDirectory = $InstallDir
    $Shortcut.WindowStyle = 7  # minimized (EXE is already windowless)
    $Shortcut.Description = "QR / barcode serial reader"
    $Shortcut.Save()
}

Write-Host ""
Write-Host "Installed: $ExeDst"
Write-Host "Config:    $PortFile"
Write-Host "Start now: Start-Process `"$ExeDst`""
if (-not $NoStartup) {
    Write-Host "Autostart: enabled (Startup folder shortcut)"
} else {
    Write-Host "Autostart: skipped (-NoStartup)"
}

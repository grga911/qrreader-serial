# Build standalone qrreader.exe with PyInstaller (Windows).
# Run from an elevated or normal PowerShell / cmd in the repo root:
#   .\scripts\build-pyinstaller.ps1
# Or:
#   scripts\build-pyinstaller.bat

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $RootDir

$VenvDir = if ($env:VENV_DIR) { $env:VENV_DIR } else { ".venv-build" }
$Python = if ($env:PYTHON) { $env:PYTHON } else { "python" }

function Assert-Command($Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $Name. Install Python 3 from https://www.python.org/ and ensure it is on PATH."
    }
}

Assert-Command $Python

Write-Host "==> Creating venv: $VenvDir"
& $Python -m venv $VenvDir

$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
$VenvPip = Join-Path $VenvDir "Scripts\pip.exe"
$PyInstaller = Join-Path $VenvDir "Scripts\pyinstaller.exe"

if (-not (Test-Path $VenvPython)) {
    throw "venv python missing: $VenvPython"
}

Write-Host "==> Upgrading pip / wheel"
& $VenvPython -m pip install --upgrade pip wheel

Write-Host "==> Installing requirements"
& $VenvPip install -r requirements-build.txt

Write-Host "==> Removing non-required packages (non-requirements.txt)"
# pip writes "Skipping X as it is not installed" to stderr; with
# $ErrorActionPreference=Stop that becomes a terminating NativeCommandError.
# Match Linux build: uninstall if present, otherwise ignore (|| true).
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    Get-Content "non-requirements.txt" | ForEach-Object {
        $pkg = $_.Trim()
        if (-not $pkg -or $pkg.StartsWith("#")) {
            return
        }
        & $VenvPython -m pip uninstall -y --disable-pip-version-check $pkg *>$null
    }
} finally {
    $ErrorActionPreference = $prevEap
}

Write-Host "==> PyInstaller build (qrreader.spec)"
if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
if (Test-Path "dist") { Remove-Item -Recurse -Force "dist" }

& $PyInstaller --clean --noconfirm qrreader.spec
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller failed with exit code $LASTEXITCODE"
}

$ExePath = Join-Path $RootDir "dist\qrreader.exe"
if (-not (Test-Path $ExePath)) {
    throw "Build failed: dist\qrreader.exe not found"
}

$size = (Get-Item $ExePath).Length
$sizeMb = [math]::Round($size / 1MB, 2)

Write-Host ""
Write-Host "Built: $ExePath ($sizeMb MB)"
Write-Host "Install / autostart: .\scripts\install-pyinstaller.ps1"
Write-Host "Run once:             .\dist\qrreader.exe COM21"
Write-Host "Port config:          %APPDATA%\qrreader\port  (one line, e.g. COM21)"

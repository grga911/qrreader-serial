# -*- mode: python ; coding: utf-8 -*-
import os
import sys

block_cipher = None

# Linux systemd: console=True so logs go to journalctl
# Windows: console=False (windowless, like running the .pyw)
_is_linux = sys.platform.startswith('linux')
_is_windows = sys.platform.startswith('win')
console = _is_linux

_icon = ['readerCOM21.ico'] if os.path.isfile('readerCOM21.ico') else None

# Bundle VERSION next to the frozen binary so --version works.
_datas = []
if os.path.isfile('VERSION'):
    _datas.append(('VERSION', '.'))

# Keep EXE small: drop unused stdlib and build-time packages
_excludes = [
    'tkinter',
    'matplotlib',
    'numpy',
    'pandas',
    'scipy',
    'PIL',
    'cv2',
    'pytest',
    'IPython',
    'pyautogui',
    'Xlib',
    'PyInstaller',
    'setuptools',
    'pkg_resources',
    'wheel',
    'pip',
    'asyncio',
    'unittest',
    'test',
    'tests',
    'xml',
    'xmlrpc',
    'html',
    'sqlite3',
    'lib2to3',
    'pydoc',
    'doctest',
    'curses',
    'multiprocessing',
    'distutils',
    'idlelib',
    'turtledemo',
    'ensurepip',
    'venv',
    'bdb',
    'pdb',
]

_hiddenimports = [
    'serial',
    'pyperclipfix',
]

a = Analysis(
    ['qrreader.pyw'],
    pathex=[],
    binaries=[],
    datas=_datas,
    hiddenimports=_hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=_excludes,
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='qrreader',
    debug=False,
    bootloader_ignore_signals=False,
    # strip is a Unix toolchain feature; skip on Windows
    strip=_is_linux,
    upx=_is_windows,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=console,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=_icon,
)

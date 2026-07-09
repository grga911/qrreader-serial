#!/usr/bin/python3
import platform
from serial import Serial, SerialException
from time import sleep
import time
import pyperclipfix as pyperclip
import codecs
import os
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

SR_CYR_TO_LATIN = {
    "А": "A",
    "а": "a",
    "Б": "B",
    "б": "b",
    "В": "V",
    "в": "v",
    "Г": "G",
    "г": "g",
    "Д": "D",
    "д": "d",
    "Ђ": "Đ",
    "ђ": "đ",
    "Е": "E",
    "е": "e",
    "Ж": "Ž",
    "ж": "ž",
    "З": "Z",
    "з": "z",
    "И": "I",
    "и": "i",
    "Ј": "J",
    "ј": "j",
    "К": "K",
    "к": "k",
    "Л": "L",
    "л": "l",
    "Љ": "Lj",
    "љ": "lj",
    "М": "M",
    "м": "m",
    "Н": "N",
    "н": "n",
    "Њ": "Nj",
    "њ": "nj",
    "О": "O",
    "о": "o",
    "П": "P",
    "п": "p",
    "Р": "R",
    "р": "r",
    "С": "S",
    "с": "s",
    "Т": "T",
    "т": "t",
    "Ћ": "Ć",
    "ћ": "ć",
    "У": "U",
    "у": "u",
    "Ф": "F",
    "ф": "f",
    "Х": "H",
    "х": "h",
    "Ц": "C",
    "ц": "c",
    "Ч": "Č",
    "ч": "č",
    "Џ": "Dž",
    "џ": "dž",
    "Ш": "Š",
    "ш": "š",
    ":": ">",
    "|": "#"
}
replace_dict = str.maketrans(SR_CYR_TO_LATIN)
# Kontrolni karakteri (CTRL, ALT, ESC....)
def remove_control_chars(s):
    CONTROL_CHARS = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\t\x0b\x0c\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
# all_chars = (chr(i) for i in range(sys.maxunicode))
# Kontrolni karakteri (CTRL, ALT, ESC....)
# control_chars = ''.join(map(chr, itertools.chain(range(0x00,0x0a),range(0x0b,0x0d),range(0x0e,0x20), range(0x7f,0xa0))))
    control_char_re = re.compile("[%s]" % re.escape(CONTROL_CHARS))
    return control_char_re.sub("", s)


last_position = -1


def mixed_decoder(unicode_error):
    global last_position
    # Karakter sa pogresnim enkodiranjem
    decoding_errorr_str = unicode_error.object[unicode_error.start : unicode_error.end]
    position = unicode_error.start
    # if position <= last_position:
    #    position = last_position + 1
    # last_position = position
    new_char = decoding_errorr_str.decode("cp1252")
    # new_char = u"_"
    return new_char, position + 1


# Registrovanje dekodera za pogrešne karaktere
codecs.register_error("mixed", mixed_decoder)


def _load_version() -> str:
    script_dir = Path(__file__).resolve().parent
    for candidate in (
        script_dir / "VERSION",
        Path("/usr/share/qrreader/VERSION"),
        Path("/usr/lib/qrreader/VERSION"),
    ):
        if candidate.is_file():
            return candidate.read_text(encoding="utf-8").strip()
    return "0.0.0"


__version__ = _load_version()


def _normalize_for_clipboard_compare(s: str) -> str:
    """Some clipboard stacks tweak line endings or trailing newlines."""
    return s.replace("\r\n", "\n").rstrip("\n\r")


def wait_clipboard_matches(expected: str, timeout_sec: float = 2.0, poll_sec: float = 0.03) -> bool:
    """
    Poll pyperclip until the clipboard matches expected.
    Avoids Ctrl+V pasting stale content when the OS has not applied copy yet.
    """
    want = _normalize_for_clipboard_compare(expected)
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        try:
            got = _normalize_for_clipboard_compare(pyperclip.paste())
        except Exception:
            got = ""
        if got == want:
            return True
        time.sleep(poll_sec)
    return False


def _read_port_config_file(path: Path) -> str:
    if not path.is_file():
        return ""
    line = path.read_text(encoding="utf-8").strip().splitlines()
    if not line:
        return ""
    return line[0].strip()


def resolve_com_port() -> str:
    OS = platform.system()
    default = "COM21" if "win" in OS.lower() else "/dev/ttyACM0"
    for path in (
        Path("/etc/qrreader/port"),
        Path.home() / ".config" / "qrreader" / "port",
    ):
        configured = _read_port_config_file(path)
        if len(configured) >= 3:
            return configured
    return default


def get_update_check_url() -> str:
    for path in (
        Path("/etc/qrreader/update.url"),
        Path.home() / ".config" / "qrreader" / "update.url",
    ):
        if path.is_file():
            url = path.read_text(encoding="utf-8").strip().splitlines()
            if url and url[0].strip():
                return url[0].strip()
    return ""


def fetch_latest_version(url: str) -> str:
    with urllib.request.urlopen(url, timeout=10) as response:
        body = response.read().decode("utf-8", errors="replace").strip()
    return body.splitlines()[0].strip() if body else ""


def version_tuple(version: str):
    parts = []
    for piece in version.split("."):
        try:
            parts.append(int(piece))
        except ValueError:
            parts.append(0)
    return tuple(parts)


def check_for_updates() -> int:
    url = get_update_check_url()
    if not url:
        print(
            "No update URL configured.\n"
            "Set /etc/qrreader/update.url (one line: URL to a plain-text VERSION file).\n"
            "When installed from .deb: sudo apt update && sudo apt upgrade qrreader",
            flush=True,
        )
        return 2

    try:
        latest = fetch_latest_version(url)
    except urllib.error.URLError as exc:
        print(f"Update check failed: {exc}", flush=True)
        return 1

    print(f"Installed: {__version__}", flush=True)
    print(f"Latest:    {latest}", flush=True)
    if version_tuple(latest) > version_tuple(__version__):
        print("Update available.", flush=True)
        print("If installed via apt: sudo apt update && sudo apt upgrade qrreader", flush=True)
        return 10
    print("You are up to date.", flush=True)
    return 0


def simulate_ctrl_v() -> bool:
    """Paste clipboard via OS tools (xdotool on Linux, ctypes on Windows)."""
    if sys.platform.startswith("linux"):
        xdotool = shutil.which("xdotool")
        if not xdotool:
            print("qrreader: xdotool not found; install: apt install xdotool", flush=True)
            return False
        try:
            result = subprocess.run(
                [xdotool, "key", "ctrl+v"],
                env=os.environ,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=5,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired) as exc:
            print(f"qrreader: xdotool failed: {exc}", flush=True)
            return False
        if result.returncode != 0:
            err = result.stderr.decode("utf-8", errors="replace").strip()
            print(f"qrreader: xdotool exited {result.returncode}: {err}", flush=True)
            return False
        return True

    if sys.platform.startswith("win"):
        try:
            import ctypes

            user32 = ctypes.windll.user32
            VK_CONTROL = 0x11
            VK_V = 0x56
            KEYEVENTF_KEYUP = 0x0002
            user32.keybd_event(VK_CONTROL, 0, 0, 0)
            user32.keybd_event(VK_V, 0, 0, 0)
            user32.keybd_event(VK_V, 0, KEYEVENTF_KEYUP, 0)
            user32.keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0)
            return True
        except Exception as exc:
            print(f"qrreader: paste hotkey failed: {exc}", flush=True)
            return False

    print(f"qrreader: paste hotkey not supported on {sys.platform}", flush=True)
    return False


def print_usage():
    print(
        """Usage:
  qrreader [COM_PORT]
  qrreader --version
  qrreader --check-update

Examples:
  qrreader COM21
  qrreader /dev/ttyACM0

Config (Linux package):
  /etc/qrreader/port
  /etc/qrreader/update.url"""
    )


if __name__ == "__main__":
    if len(sys.argv) >= 2:
        arg = sys.argv[1]
        if arg in ("--version", "-V"):
            print(__version__)
            raise SystemExit(0)
        if arg == "--check-update":
            raise SystemExit(check_for_updates())

    COM_PORT = resolve_com_port()
    if len(sys.argv) >= 2 and not sys.argv[1].startswith("-"):
        COM_PORT = sys.argv[1]
        if len(COM_PORT) < 3:
            print_usage()
            raise SystemExit(1)
    while True:
        try:
            with Serial(COM_PORT, 9600, 8, "N", 1) as s:
                print(f"serial_connected {COM_PORT}", flush=True)
                s.reset_input_buffer()

                while True:
                    try:
                        if s.in_waiting <= 4:
                            sleep(1)
                            continue

                        result = s.read_all()
                        s.reset_output_buffer()
                        s.reset_input_buffer()

                        old_clipboard = pyperclip.paste()

                        result = result.replace(b"\r\n", b"#")
                        result = result.replace(b"\n", b"#")
                        result = result.replace(b"\r", b"#")
                        copy_result = remove_control_chars(
                            (result.decode(errors="mixed").strip().translate(replace_dict))
                        )
                        sleep(0.2)

                        settled = False
                        for _ in range(3):
                            pyperclip.copy(copy_result)
                            if wait_clipboard_matches(copy_result, timeout_sec=1.0):
                                settled = True
                                break
                            sleep(0.05)

                        if not settled:
                            print(
                                "qrreader: clipboard did not update to scan text; "
                                "skipping paste (avoids pasting stale clipboard).",
                                flush=True,
                            )
                            try:
                                pyperclip.copy(old_clipboard)
                            except Exception:
                                pass
                            continue

                        sleep(0.05)
                        if not simulate_ctrl_v():
                            try:
                                pyperclip.copy(old_clipboard)
                            except Exception:
                                pass
                            continue
                        sleep(0.2)
                        pyperclip.copy(old_clipboard)
                        sleep(0.2)
                    except (SerialException, OSError) as e:
                        print(f"serial_disconnected {COM_PORT}: {e}", flush=True)
                        break

                print(f"serial_reconnecting {COM_PORT}", flush=True)
        except (SerialException, OSError) as e:
            print(f"serial_open_failed {COM_PORT}: {e}", flush=True)
            sleep(1)
            continue
        except Exception as e:
            print(e, flush=True)
            sleep(1)
            continue

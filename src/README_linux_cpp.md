# qrreader Linux C++ rewrite

This is a Linux C++ rewrite of `qrreader.pyw`.

## Dependencies

- `g++` (or `clang++`)
- `cmake`
- `xclip` (clipboard read/write)
- `xdotool` (simulate Ctrl+V)

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake xclip xdotool
```

## Build

```bash
cmake -S . -B build-linux
cmake --build build-linux --config Release
```

### Static linking (portability)

This program only links the C++ runtime and libc (no OpenCV, etc.). Two levels:

1. **Static C++ runtime (usual choice)** — embed `libstdc++` and `libgcc` so the binary does not depend on the host’s `libstdc++.so.6` version. **glibc stays dynamic** (normal for portable Linux binaries).

   ```bash
   cmake -S . -B build-linux -DQRREADER_STATIC_LIBSTDCPP=ON
   cmake --build build-linux --config Release
   ```

2. **Fully static** — entire libc linked in. With **glibc**, `-static` is often unreliable on real systems; use a **musl** toolchain (e.g. Alpine `apk add build-base cmake`, or `x86_64-linux-musl-g++` from your distro / cross packages) and then:

   ```bash
   cmake -S . -B build-musl -DQRREADER_FULLY_STATIC=ON
   cmake --build build-musl --config Release
   ```

   Confirm with `ldd build-musl/qrreader_linux` → `not a dynamic executable` (musl) or no unexpected libs.

**Still required at runtime:** `xclip` and `xdotool` are separate programs invoked by this binary; static linking does not bundle them. Ship them in the same tarball / PATH, or document `apt install xclip xdotool`.

## Run

Default port:

```bash
./build-linux/qrreader_linux
```

Custom serial port:

```bash
./build-linux/qrreader_linux /dev/ttyACM0
```

Debug logging is enabled by default (timestamps + clipboard/paste values):

```bash
./build-linux/qrreader_linux /dev/ttyACM0
```

Logs are printed to stderr in this form:

```text
[2026-05-20 12:00:00.123] clipboard_before="..."
[2026-05-20 12:00:00.456] pasted_text="..."
[2026-05-20 12:00:00.789] clipboard_cleared="..."
```

## Behavior parity with Python version

- Reads serial data at `9600 8N1`
- Waits until more than 4 bytes are available
- Replaces `CR/LF` with `#`
- Uses mixed decode strategy (UTF-8 + CP1252 fallback)
- Removes control characters
- Applies Serbian Cyrillic to Latin transliteration plus:
  - `:` -> `>`
  - `|` -> `#`
- Writes decoded text, sends `Ctrl+V`, then clears the clipboard (never restores the previous clipboard)

## Stale serial data (wrong paste)

If the pasted text did not match the last scan, common causes were:

1. **Clipboard read before serial read** — `pyperclip.paste()` / `xclip` can be slow; more bytes could arrive or the snapshot felt “wrong”. The Python script and this program now **read the serial port first**, then read the previous clipboard.
2. **Leftover bytes in the driver** — especially because the Python version opens the port on every scan. Both versions now **flush the serial input buffer when the port opens** so old data is not pasted as the next scan.

### Old clipboard pasted (Ctrl+V race)

Some desktops apply clipboard updates asynchronously. If `Ctrl+V` runs before the new text is visible to the app receiving the paste, the **previous** clipboard content is pasted.

`qrreader.pyw` and `qrreader_linux.cpp` now **poll the clipboard until a read-back matches the scan text** (with a few copy retries and a timeout) before sending `Ctrl+V`. If it never matches, they **skip paste** and clear/restore safely instead of pasting wrong data.

### Sticky previous scan (Merkur / barcode loop)

Field logs showed this pattern:

1. A scan (e.g. Merkur IPS QR) is pasted correctly while `Old` was empty.
2. Restoring an empty clipboard with `pyperclip.copy('')` / empty `xclip` stdin **does not clear** on many Linux desktops, so that scan remains as clipboard content.
3. Every later scan then has that text as `Old`. Restoring `Old` ~200ms after `Ctrl+V` races the target app, so **Merkur (or a barcode) is pasted instead of the new scan**, and `Old` stays stuck for hours.

Mitigations now:

- **Never write the previous clipboard (`Old`) back.** This removes the sticky-paste by construction: a previous slip can no longer be on the clipboard when the app reads the paste, regardless of timing.
- After a successful paste, wait ~800ms (so the banking UI can consume `New`), then **overwrite** the clipboard (and PRIMARY selection) with a space via `xclip`/`xsel -i`. This — instead of leaving `New` — also prevents an accidental manual `Ctrl+V` from repeating the last slip.
  - We **overwrite, never empty**: emptying the selection makes a clipboard manager (GNOME, Klipper, CopyQ, Parcellite, …) restore the previous entry, which would let the user's earlier copy (e.g. `1234`) be pasted again after a scan.
- Re-check that the clipboard still matches `New` immediately before `Ctrl+V`.
- **Rescanning the same barcode still works**: each scan re-copies and re-verifies its own text, so there is no duplicate-scan suppression.
- Log `Old` / `New` with timestamps for journal diagnosis.

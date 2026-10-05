#!/usr/bin/env python3
"""Bildschirmfoto vom Board über USB (nur zur Fehlersuche).

Schickt "S" an das Board (config.h: SCREENSHOT_SERIAL) und speichert das Bild als PNG.
Aufruf:  python tools/screenshot.py COM7 bild.png
Braucht pyserial (in PlatformIO enthalten) und schreibt das PNG ohne weitere Bibliotheken.
"""
import struct
import sys
import time
import zlib

import serial


def read_exact(s, n, timeout=10):
    data = bytearray()
    end = time.time() + timeout
    while len(data) < n and time.time() < end:
        data += s.read(n - len(data))
    if len(data) < n:
        raise SystemExit(f"nur {len(data)} von {n} Bytes empfangen")
    return bytes(data)


def png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    port, out = sys.argv[1], sys.argv[2]
    s = serial.Serial(port, 115200, timeout=0.5)
    s.reset_input_buffer()
    s.write(b"S")
    end = time.time() + 10
    line = b""
    while time.time() < end:
        line = s.readline()
        if line.startswith(b"SHOT "):
            break
    else:
        raise SystemExit("keine Antwort vom Board")
    w, h = map(int, line.split()[1:3])
    scr = read_exact(s, w * h * 2)
    top = read_exact(s, w * h * 4)
    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        v = scr[2 * i] | (scr[2 * i + 1] << 8)
        r, g, b = (v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3
        tb, tg, tr, ta = top[4 * i:4 * i + 4]
        rgb[3 * i] = (tr * ta + r * (255 - ta)) // 255
        rgb[3 * i + 1] = (tg * ta + g * (255 - ta)) // 255
        rgb[3 * i + 2] = (tb * ta + b * (255 - ta)) // 255
    png(out, w, h, bytes(rgb))
    print("gespeichert:", out)


if __name__ == "__main__":
    main()

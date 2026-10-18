#!/usr/bin/env python3
"""Erzeugt kantengeglättete VLW-Schriften (Format von TFT_eSPI/LovyanGFX) als C-Header.

Aufruf: python3 tools/make_vlw.py  (benötigt Pillow und DejaVu Sans, z. B. aus matplotlib)
"""
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "include" / "fonts"

ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
TEXT = ASCII + "ÄÖÜäöüß°·–…"
DIGITS = " -+0123456789°:.%"

# (Name, TTF-Datei, Pixelgröße, Zeichensatz)
FONTS = [
    ("font_small", "DejaVuSans.ttf", 18, TEXT),
    ("font_text", "DejaVuSans.ttf", 24, TEXT),
    ("font_bold", "DejaVuSans-Bold.ttf", 30, TEXT),
    ("font_clock", "DejaVuSans-Bold.ttf", 52, DIGITS),
    ("font_huge", "DejaVuSans-Bold.ttf", 112, DIGITS),
]


def find_ttf(name):
    import matplotlib

    path = Path(matplotlib.get_data_path()) / "fonts" / "ttf" / name
    if not path.exists():
        sys.exit(f"{name} nicht gefunden")
    return path


def make_vlw(ttf, size, chars):
    font = ImageFont.truetype(str(ttf), size)
    headers, bitmaps = [], []
    for ch in sorted(set(chars), key=ord):
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = max(x1 - x0, 0), max(y1 - y0, 0)
        advance = round(font.getlength(ch))
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
            data = img.tobytes()
        else:
            x0 = y0 = 0
            data = b""
        assert w < 256 and advance < 256 and -128 <= x0 < 128, ch
        # unicode, Höhe, Breite, xAdvance, Abstand Oberkante-Grundlinie, x-Versatz, Füllfeld
        headers.append(struct.pack(">7i", ord(ch), h, w, advance, -y0, x0, 0))
        bitmaps.append(data)

    ascent = -font.getbbox("d", anchor="ls")[1]
    descent = font.getbbox("p", anchor="ls")[3]
    head = struct.pack(">6i", len(headers), 11, size, 0, ascent, descent)
    return head + b"".join(headers) + b"".join(bitmaps)


def write_header(name, blob, path):
    lines = [", ".join(f"0x{b:02x}" for b in blob[i:i + 20]) for i in range(0, len(blob), 20)]
    path.write_text(
        "// Automatisch erzeugt von tools/make_vlw.py - nicht von Hand bearbeiten\n"
        "// Schrift: DejaVu Sans (Bitstream Vera License)\n"
        "#pragma once\n#include <stdint.h>\n\n"
        f"static const uint8_t {name}[{len(blob)}] = {{\n  " + ",\n  ".join(lines) + "\n};\n"
    )


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, ttf, size, chars in FONTS:
        blob = make_vlw(find_ttf(ttf), size, chars)
        write_header(name, blob, OUT_DIR / f"{name}.h")
        print(f"{name}: {len(blob) / 1024:.1f} KB")


if __name__ == "__main__":
    main()

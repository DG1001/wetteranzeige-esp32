#!/usr/bin/env python3
"""Holt ein Bildschirmfoto vom Board über die serielle Schnittstelle und speichert es als PNG.

Aufruf: python3 tools/screenshot.py /dev/cu.usbserial-XXX bild.png [befehl ...]
Optionale Befehle (z. B. "seite 1") werden vorher geschickt, jeweils mit 2 s Pause.
Benötigt pyserial und Pillow.
"""
import sys
import time

import serial
from PIL import Image


def main():
    port, out, commands = sys.argv[1], sys.argv[2], sys.argv[3:]
    # DTR/RTS vor dem Öffnen setzen, sonst startet das Board neu
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 2
    s.dtr = False
    s.rts = False
    s.open()
    time.sleep(0.2)
    for cmd in commands:
        s.write((cmd + "\n").encode())
        time.sleep(2.5)
    s.reset_input_buffer()
    s.write(b"foto\n")
    deadline = time.time() + 10
    header = b""
    while time.time() < deadline:
        header = s.readline()
        if header.startswith(b"FOTO"):
            break
    else:
        sys.exit("keine Antwort vom Board")
    _, w, h = header.split()
    w, h = int(w), int(h)
    s.baudrate = 460800
    time.sleep(0.2)
    s.reset_input_buffer()
    s.write(b"G")
    data = bytearray()
    deadline = time.time() + 30
    while len(data) < w * h * 2 and time.time() < deadline:
        data += s.read(w * h * 2 - len(data))
    s.baudrate = 115200
    if len(data) != w * h * 2:
        sys.exit(f"unvollständig: {len(data)} von {w * h * 2} Bytes")

    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        hi, lo = data[2 * i], data[2 * i + 1]  # LovyanGFX: erstes Byte RRRRRGGG, zweites GGGBBBBB
        v = hi << 8 | lo
        r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        px[i % w, i // w] = (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)
    img.save(out)
    print(f"{out} gespeichert")


if __name__ == "__main__":
    main()

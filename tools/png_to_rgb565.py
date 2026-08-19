#!/usr/bin/env python3
"""Convert an 800x480 PNG to little-endian RGB565 for the ESP32-S3 panel."""

import argparse
import shutil
import struct
import subprocess
from pathlib import Path


WIDTH = 800
HEIGHT = 480


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    magick = shutil.which("magick")
    if magick is None:
        raise SystemExit("ImageMagick 'magick' is required")

    size = subprocess.check_output(
        [magick, "identify", "-format", "%wx%h", str(args.input)], text=True
    )
    if size != f"{WIDTH}x{HEIGHT}":
        raise SystemExit(f"expected {WIDTH}x{HEIGHT}, got {size}")

    rgb = subprocess.check_output(
        [magick, str(args.input), "-alpha", "off", "-depth", "8", "rgb:-"]
    )
    expected_bytes = WIDTH * HEIGHT * 3
    if len(rgb) != expected_bytes:
        raise SystemExit(f"expected {expected_bytes} RGB bytes, got {len(rgb)}")

    output = bytearray(WIDTH * HEIGHT * 2)
    for pixel in range(WIDTH * HEIGHT):
        red, green, blue = rgb[pixel * 3 : pixel * 3 + 3]
        rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        struct.pack_into("<H", output, pixel * 2, rgb565)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    print(f"wrote {args.output} ({len(output)} bytes)")


if __name__ == "__main__":
    main()

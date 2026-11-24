"""Validate and pack the original HellForge RGBA sprites. Requires Python 3 only."""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1] / "duct-tape-doom-main"
ART = ROOT / "assets/gibs"
OUTPUT = ROOT / "platform/scene3d/giblet_atlas.generated.h"
SIGNATURE = b"\x89PNG\r\n\x1a\n"
NAMES = [f"GIB{i}{pose}0" for i in range(10) for pose in "AB"]
NAMES += [f"CRS{i}A0" for i in range(1, 4)]
NAMES += [f"SG{side}{i}{pose}0" for side in "CW" for i in (1, 2) for pose in "AB"]
NAMES += ["SQSHA0"]
MATERIAL = [1, 1, 0, 0, 2, 2, 3, 3, 0, 3]  # Matches giblets::Kind.
WIDTH = 256
GUTTER = 2


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def decode(data):
    if not data.startswith(SIGNATURE):
        raise ValueError("invalid PNG signature")
    pos, compressed, header = 8, bytearray(), None
    while pos + 12 <= len(data):
        size, kind = struct.unpack_from(">I4s", data, pos)
        end = pos + 8 + size
        if end + 4 > len(data):
            raise ValueError("truncated PNG chunk")
        chunk = data[pos + 8:end]
        if zlib.crc32(kind + chunk) != struct.unpack_from(">I", data, end)[0]:
            raise ValueError("bad PNG chunk checksum")
        if kind == b"IHDR":
            if pos != 8 or size != 13:
                raise ValueError("invalid IHDR ordering or size")
            w, h, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk)
            if not (0 < w <= 128 and 0 < h <= 128 and (depth, color, compression, filtering, interlace) == (8, 6, 0, 0, 0)):
                raise ValueError("expected bounded, noninterlaced 8-bit RGBA")
            header = (w, h)
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            if size or end + 4 != len(data):
                raise ValueError("invalid PNG ending")
            break
        elif not kind[0] & 32:
            raise ValueError("unexpected critical PNG chunk")
        pos = end + 4
    else:
        raise ValueError("missing IEND")
    if not header:
        raise ValueError("missing IHDR")
    w, h = header
    stride = w * 4
    length = (stride + 1) * h
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, length + 1)
    if len(raw) != length or not decoder.eof or decoder.unused_data:
        raise ValueError("bad image data length")
    pixels = bytearray(stride * h)
    for y in range(h):
        predictor = raw[y * (stride + 1)]
        if predictor > 4:
            raise ValueError("invalid PNG filter")
        for x in range(stride):
            i = y * stride + x
            a, b = (pixels[i - 4] if x >= 4 else 0), (pixels[i - stride] if y else 0)
            c = pixels[i - stride - 4] if y and x >= 4 else 0
            bias = (0, a, b, (a + b) // 2, paeth(a, b, c))[predictor]
            pixels[i] = (raw[y * (stride + 1) + x + 1] + bias) & 255
    alpha = pixels[3::4]
    if set(alpha) != {0, 255}:
        raise ValueError("expected binary alpha, transparent and opaque texels")
    points = [divmod(i, w) for i, a in enumerate(alpha) if a]
    ys, xs = zip(*points)
    return w, h, pixels, (min(xs), min(ys), max(xs) + 1, max(ys) + 1)


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def pack():
    actual = {path.name for path in ART.glob("*.png")}
    expected = {name + ".png" for name in NAMES}
    if actual != expected:
        raise ValueError(f"missing {sorted(expected - actual)}, unexpected {sorted(actual - expected)}")
    assets = []
    inventory = []
    for name in NAMES:
        path = ART / (name + ".png")
        if path.stat().st_size > 65536:
            raise ValueError(f"{name}: oversized PNG")
        data = path.read_bytes()
        try:
            info = decode(data)
        except (ValueError, zlib.error) as exc:
            raise ValueError(f"{name}: {exc}") from exc
        assets.append(info)
        inventory.append(f"{name} {info[0]:3}x{info[1]:<3} {hashlib.sha256(data).hexdigest()}")

    entries = []
    x = y = GUTTER
    row_height = 0
    for w, h, _, (left, top, right, bottom) in assets:
        rw, rh = right - left, bottom - top
        if x + rw + GUTTER > WIDTH:
            x, y = GUTTER, y + row_height + 2 * GUTTER
            row_height = 0
        entries.append((x, y, rw, rh))
        x += rw + 2 * GUTTER
        row_height = max(row_height, rh)
    height = y + row_height + GUTTER
    pixels = bytearray(WIDTH * height * 4)
    for (x, y, rw, rh), (w, h, rgba, (left, top, _, _)) in zip(entries, assets):
        for row in range(rh):
            dst = ((y + row) * WIDTH + x) * 4
            src = ((top + row) * w + left) * 4
            pixels[dst:dst + rw * 4] = rgba[src:src + rw * 4]
    raw = b"".join(b"\0" + pixels[i:i + WIDTH * 4] for i in range(0, len(pixels), WIDTH * 4))
    png = SIGNATURE + png_chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, height, 8, 6, 0, 0, 0))
    png += png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b"")
    header = ["// Generated by tools/pack_giblets.py. Do not edit.", "#pragma once", "",
              "namespace doom3d::giblet_art {",
              "struct Frame {",
              "    const char *id;",
              "    int x,y,w,h,canvasW,canvasH,left,top,kind,pose;",
              "    float pivotX,pivotY,scale;",
              "};",
              f"inline constexpr int width={WIDTH}, height={height};",
              "inline constexpr Frame frames[]={"]
    for i, (name, (x, y, rw, rh), (w, h, _, (left, top, _, _))) in enumerate(zip(NAMES, entries, assets)):
        kind, pose = (MATERIAL[i // 2], i % 2) if i < 20 else (-1, 2)
        # Pose pairs share a canonical apparent size. Settled art spreads the
        # same piece across the floor, so its footprint is slightly wider.
        scale = 1.15 if pose == 1 else 1.0
        header.append(f'    {{"{name}",{x},{y},{rw},{rh},{w},{h},{left},{top},{kind},{pose},.5f,.5f,{scale:.2f}f}},')
    header += ["};", "inline constexpr unsigned char png[]={"]
    header += ["    " + ",".join(str(b) for b in png[i:i + 24]) + "," for i in range(0, len(png), 24)]
    header += ["};", "}", ""]
    return "\n".join(header), inventory, (WIDTH, height)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="validate the checked-in atlas")
    args = parser.parse_args()
    try:
        content, inventory, dimensions = pack()
        if args.check:
            if OUTPUT.read_text(encoding="ascii") != content:
                raise ValueError("embedded atlas is stale, run tools/pack_giblets.py")
        else:
            OUTPUT.write_text(content, encoding="ascii", newline="\n")
    except (ValueError, OSError) as exc:
        parser.exit(1, f"Giblet art: {exc}\n")
    print("\n".join(inventory))
    print(f"{len(inventory)} sprites, {dimensions[0]}x{dimensions[1]} embedded atlas")

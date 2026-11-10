"""Generated big-texture and malformed-XNOD tests through the real engine."""
import argparse
from pathlib import Path
import struct
import subprocess


def wad(path, lumps):
    data = bytearray(b"PWAD" + bytes(8))
    entries = []
    for name, contents in lumps:
        entries.append((len(data), len(contents), name.encode().ljust(8, b"\0")))
        data.extend(contents)
    offset = len(data)
    for entry in entries:
        data.extend(struct.pack("<ii8s", *entry))
    struct.pack_into("<ii", data, 4, len(entries), offset)
    path.write_bytes(data)


def patch(width, height, color):
    column = bytearray()
    previous, y = -1, 0
    while y < height:
        count = min(200, height - y)
        top = y if y <= 254 and y > previous else y - previous
        column.extend(bytes((top, count, 0)) + bytes([color]) * count + b"\0")
        previous, y = y, y + count
    column.append(255)
    base = 8 + width * 4
    return (struct.pack("<hhhh", width, height, 0, height) +
            b"".join(struct.pack("<I", base + x * len(column)) for x in range(width)) +
            column * width)


def texture(name, width, height, parts):
    return (struct.pack("<8sihhih", name.encode(), 0, width, height, 0, len(parts)) +
            b"".join(struct.pack("<hhhhh", x, y, p, 0, 0) for x, y, p in parts))


def visual_fixture(sky=False):
    """Two rooms with a real translucent middle wall, using IWAD artwork."""
    points = [(-128, -128), (-128, 0), (-128, 128), (128, 128), (128, 0), (128, -128)]
    defs = [(0, 1, 0), (1, 2, 1), (2, 3, 1), (3, 4, 1), (4, 5, 0), (5, 0, 0)]
    lines = bytearray()
    sides = bytearray()
    for index, (a, b, sector) in enumerate(defs):
        lines.extend(struct.pack("<7H", a, b, 1, 0, 0, index, 65535))
        sides.extend(struct.pack("<hh8s8s8sh", 0, 0, b"-", b"-", b"STARTAN3", sector))
    lines.extend(struct.pack("<7H", 1, 4, 4, 271 if sky else 260, 7 if sky else 0, 6, 7))
    for sector in (0, 1):
        sides.extend(struct.pack("<hh8s8s8sh", 0, 0, b"SKY2" if sky else b"-", b"-", b"-" if sky else b"STARTAN3", sector))
    segs = bytearray()
    for a, b, line, side in [(0, 1, 0, 0), (4, 5, 4, 0), (5, 0, 5, 0), (1, 4, 6, 0),
                              (1, 2, 1, 0), (2, 3, 2, 0), (3, 4, 3, 0), (4, 1, 6, 1)]:
        segs.extend(struct.pack("<6H", a, b, 0, line, side, 0))
    nodes = struct.pack("<12h2H", -128, 0, 256, 0, 0, -128, -128, 128,
                        128, 0, -128, 128, 0x8000, 0x8001)
    sector = struct.pack("<hh8s8shhh", 0, 128, b"FLOOR0_1", b"F_SKY1" if sky else b"CEIL1_1", 255, 0, 7 if sky else 0)
    return [("MAP01", b""), ("THINGS", struct.pack("<10h", 0, -64, 90, 1, 7, 0, 64, 0, 2028, 7)),
            ("LINEDEFS", bytes(lines)), ("SIDEDEFS", bytes(sides)),
            ("VERTEXES", b"".join(struct.pack("<hh", *point) for point in points)),
            ("SEGS", bytes(segs)), ("SSECTORS", struct.pack("<4H", 4, 0, 4, 4)),
            ("NODES", nodes), ("SECTORS", sector * 2), ("REJECT", b"\0"), ("BLOCKMAP", b"")]


def main():
    ap = argparse.ArgumentParser()
    for key in ("engine", "iwad", "out"):
        ap.add_argument("--" + key, type=Path, required=True)
    args = ap.parse_args()
    args.engine, args.iwad, args.out = (p.resolve() for p in (args.engine, args.iwad, args.out))
    args.out.mkdir(parents=True, exist_ok=True)
    from bigmap_smoke import directory
    base = dict(directory(args.iwad))
    pnames = base["PNAMES"]
    count = struct.unpack_from("<I", pnames)[0]
    pnames = struct.pack("<I", count + 3) + pnames[4:] + b"BTPATCH1BTPATCH2BTPATCH3"
    textures = [
        texture("BTSTBIG", 512, 512, [(0, 0, count), (0, 0, count + 1)]),
        texture("BTSTCLIP", 512, 256, [(0, -32, count)]),
        texture("BTSTGAP", 256, 512, [(0, 0, count + 2), (192, 0, count + 2)]),
        texture("BTSTNPOT", 300, 512, [(0, 0, count), (0, 0, count + 1)]),
        texture("BTSTOVER", 8, 8, [(0, 0, count + 2)] * 300),
    ]
    offset = 4 + 4 * len(textures)
    offsets = []
    for t in textures:
        offsets.append(offset)
        offset += len(t)
    texture2 = (struct.pack("<I", len(textures)) +
                b"".join(struct.pack("<I", x) for x in offsets) + b"".join(textures))
    common = [("PNAMES", pnames), ("TEXTURE2", texture2)]
    # Force patch lump numbers beyond the old signed-short range.
    common += [(f"F{i:07}", b"") for i in range(33000)]
    common += [("BTPATCH1", patch(512, 512, 50)),
               ("BTPATCH2", patch(512, 512, 180)),
               ("BTPATCH3", patch(64, 512, 90))]
    target = args.out / "large-textures.wad"
    wad(target, common)
    checks = 0

    def run(name, lumps=None, expected=None, extra=()):
        nonlocal checks
        data = target if lumps is None else args.out / (name + ".wad")
        if lumps is not None:
            wad(data, lumps)
        work = args.out / name
        work.mkdir(exist_ok=True)
        command = [str(args.engine), "-iwad", str(args.iwad), "-file", str(data),
                   "-warp", "1", "-checkbigmap", "-checkrender"] + (list(extra) if extra else ["-heapsize", "512"])
        proc = subprocess.run(command, cwd=work, capture_output=True, timeout=40)
        output = (proc.stdout + proc.stderr).decode(errors="replace")
        (work / "engine.log").write_text(output, encoding="utf-8")
        good = (proc.returncode == 0 and "PASS bigmap:" in output) if expected is None else (
            proc.returncode != 0 and expected in output)
        if not good:
            raise AssertionError(name + "\n" + output[-1800:])
        checks += 1
        print(name, "PASS", flush=True)

    run("large-textures")
    run("translucent-world", visual_fixture())
    run("transferred-sky", visual_fixture(sky=True))
    extended_map = []
    entries = directory(args.iwad)
    map_at = next(i for i, entry in enumerate(entries) if entry[0] == "MAP01")
    for key, contents in entries[map_at:map_at + 11]:
        if key == "THINGS":
            player = next(x for x in struct.iter_unpack("<hhhhh", contents) if x[3] == 1)
            contents += struct.pack("<hhhhh", player[0] + 64, player[1], 0, 150, 7)
        extended_map.append((key, contents))
    deh = b"Patch File for DeHackEd v3.0\nDoom version = 21\nPatch format = 6\n\nThing 151\nID # = 150\nInitial frame = 1100\n\nFrame 1100\nSprite number = 146\nNext frame = 0\n"
    run("extended-sprite-namespace", extended_map + [("DEHACKED", deh), ("SS_START", b""),
        ("BRS1A0", patch(64, 128, 20)), ("SP01A0", patch(64, 128, 180)), ("SS_END", b"")])
    malformed = bytearray(texture2)
    struct.pack_into("<I", malformed, 4, 0x7fffffff)
    run("texture-directory-range", [("TEXTURE2", bytes(malformed))], "bad texture directory")
    run("texture-directory-short", [("TEXTURE2", b"\1")], "truncated TEXTURE2")
    bad_names = struct.pack("<I", 0x7fffffff)
    run("pnames-range", [("PNAMES", bad_names)], "invalid PNAMES count")
    # directory() cannot key repeated map children globally; extract MAP01 in order.
    entries = directory(args.iwad)
    at = next(i for i, entry in enumerate(entries) if entry[0] == "MAP01")
    map_lumps = entries[at:at + 11]
    original = len(dict(map_lumps)["VERTEXES"]) // 4
    node = b"XNOD" + struct.pack("<II", original, 0) + struct.pack("<I", 0x7fffffff)
    run("xnod-truncated", [(k, node if k == "NODES" else d) for k, d in map_lumps], "XNOD: invalid array count")
    line = struct.unpack_from("<7H", dict(map_lumps)["LINEDEFS"])
    xnode = (b"XNOD" + struct.pack("<IIIII", original, 0, 1, 1, 1) +
             struct.pack("<IIHB", line[0], line[1], 0, 0) + struct.pack("<I", 1) +
             bytes(24) + struct.pack("<II", 0, 0x80000000))
    run("xnod-cycle", [(k, xnode if k == "NODES" else d) for k, d in map_lumps], "BSP contains a cycle")
    bad_child = xnode[:-8] + struct.pack("<II", 0x7fffffff, 0x80000000)
    run("xnod-child-range", [(k, bad_child if k == "NODES" else d) for k, d in map_lumps], "node child")
    for value in ("0", "2048", "512junk", "-1"):
        run("heap-" + value.replace("-", "minus"), expected="Invalid -heapsize", extra=("-heapsize", value))

    work = args.out / "boom-actions"
    work.mkdir(exist_ok=True)
    for corrupt in (False, True):
        cmd = [str(args.engine), "-iwad", str(args.iwad), "-warp", "1", "-checkbigmap", "-checkboom"]
        if corrupt:
            cmd.append("-checkboomcorrupt")
        proc = subprocess.run(cmd, cwd=work, capture_output=True, timeout=30)
        output = (proc.stdout + proc.stderr).decode(errors="replace")
        (work / ("corrupt.log" if corrupt else "actions.log")).write_text(output, encoding="utf-8")
        assert (proc.returncode != 0 and "Invalid Boom save extension" in output) if corrupt else (
            proc.returncode == 0 and "PASS Boom action regressions:" in output), output[-1000:]
        checks += 1
    print(f"PASS {checks} generated engine regressions plus Boom action assertions")


if __name__ == "__main__":
    main()

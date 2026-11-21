"""Generate a Doom II render-check PWAD using only stock artwork.

Usage: python tests/selected_texture_scene.py output.wad
Maps 01–06 show the initial five wall profiles and the tentacle flat.
Map 07 is an ordinary wall and animated ceiling control. Maps 08–15
show the taller spine wall, additional leathers, and engraved marble faces.
"""

import struct
import sys
from pathlib import Path


def name(value):
    return value.encode("ascii").ljust(8, b"\0")


def room(number, wall, floor="FLOOR0_1", ceiling="CEIL3_5"):
    points = [(-128, -64), (-128, 64), (128, 64), (128, -64)]
    lines = b"".join(struct.pack("<7h", i, (i + 1) % 4, 1, 0, 0, i, -1) for i in range(4))
    sides = (struct.pack("<2h", 0, 0) + name("-") * 2 + name(wall) + struct.pack("<h", 0)) * 4
    segs = b"".join(struct.pack("<6H", i, (i + 1) % 4, angle, i, 0, 0)
                    for i, angle in enumerate((16384, 0, 49152, 32768)))
    return [
        (f"MAP{number:02}", b""),
        ("THINGS", struct.pack("<5h", 0, 0, 90, 1, 7)),
        ("LINEDEFS", lines), ("SIDEDEFS", sides),
        ("VERTEXES", b"".join(struct.pack("<2h", *p) for p in points)),
        ("SEGS", segs), ("SSECTORS", struct.pack("<2h", 4, 0)), ("NODES", b""),
        ("SECTORS", struct.pack("<2h", 0, 96) + name(floor) + name(ceiling) + struct.pack("<3h", 176, 0, 0)),
        ("REJECT", b"\0"),
        ("BLOCKMAP", struct.pack("<12h", -128, -64, 2, 1, 6, 6, 0, 0, 1, 2, 3, -1)),
    ]


def write_gallery(path):
    walls = ("SKINFACE", "SKSPINE2", "SP_FACE2", "STARTAN3", "SKINMET1", "SP_FACE1", "STARTAN3", "SKSPINE1",
             "SKINMET2", "SKINSCAB", "SKINSYMB", "SKINCUT", "MARBFACE", "MARBFAC2", "MARBFAC3")
    data, directory = bytearray(), bytearray()
    for i, wall in enumerate(walls, 1):
        floor = "SFLR6_4" if i == 4 else "FLOOR0_1"
        ceiling = "SFLR6_4" if i == 4 else "NUKAGE1" if i == 7 else "CEIL3_5"
        for label, lump in room(i, wall, floor, ceiling):
            directory += struct.pack("<ii8s", len(data) + 12, len(lump), name(label))
            data += lump
    path.write_bytes(b"PWAD" + struct.pack("<ii", len(directory) // 16, len(data) + 12) + data + directory)


if __name__ == "__main__":
    write_gallery(Path(sys.argv[1]))

"""Generate a Doom II render-check PWAD using only stock artwork.

Usage: python tests/selected_texture_scene.py output.wad [--finish | --horror]
Maps 01–06 show the initial five wall profiles and the tentacle flat.
Map 07 is an ordinary wall and animated ceiling control. Maps 08–15
show the taller spine wall, additional leathers, and engraved marble faces.
--finish generates a separate gallery for the remaining walls, flats and skies.
--horror shows the marble faces, skull eyes, and intestine walls.
"""

import struct
import sys
from pathlib import Path


def name(value):
    return value.encode("ascii").ljust(8, b"\0")


def room(number, wall, floor="FLOOR0_1", ceiling="CEIL3_5", height=96):
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
        ("SECTORS", struct.pack("<2h", 0, height) + name(floor) + name(ceiling) + struct.pack("<3h", 176, 0, 0)),
        ("REJECT", b"\0"),
        ("BLOCKMAP", struct.pack("<12h", -128, -64, 2, 1, 6, 6, 0, 0, 1, 2, 3, -1)),
    ]


def initial_rooms():
    walls = ("SKINFACE", "SKSPINE2", "SP_FACE2", "STARTAN3", "SKINMET1", "SP_FACE1", "STARTAN3", "SKSPINE1",
             "SKINMET2", "SKINSCAB", "SKINSYMB", "SKINCUT", "MARBFACE", "MARBFAC2", "MARBFAC3")
    for i, wall in enumerate(walls, 1):
        floor = "SFLR6_4" if i == 4 else "FLOOR0_1"
        ceiling = "SFLR6_4" if i == 4 else "NUKAGE1" if i == 7 else "CEIL3_5"
        yield room(i, wall, floor, ceiling)


def final_rooms():
    for i in (1, 12, 21):
        yield room(i, "STARTAN3", ceiling="F_SKY1", height=64)
    walls = {2: "SP_DUDE7", 3: "SP_DUDE8", 4: "SP_DUDE1", 5: "SP_DUDE5", 6: "BLODRIP1",
             7: "GSTFONT1", 8: "SW1SKIN", 9: "SW2SKIN", 10: "FIREBLU1", 11: "FIRELAVA",
             13: "FIREMAG1", 14: "FIREWALL", 19: "GSTFONT2", 20: "GSTFONT3",
             22: "SP_DUDE2", 23: "SP_DUDE4", 24: "STARTAN3"}
    for number, wall in walls.items():
        yield room(number, wall, height=128)
    for gate in range(1, 5):
        yield room(14 + gate, "STARTAN3", floor=f"GATE{gate}")


def horror_rooms():
    walls = ("MARBFACE", "MARBFAC2", "MARBFAC3", "SW1MARB", "SW2MARB", "WOOD4",
             "SLADSKUL", "WOODGARG", "SKSNAKE1", "SKSNAKE2", "SW2SKIN", "STARTAN3")
    for i, wall in enumerate(walls, 1):
        yield room(i, wall, height=128)
    yield room(13, "WOOD4", height=64)  # Bring the upper skull to eye level.


def write_gallery(path, rooms):
    data, directory = bytearray(), bytearray()
    for lumps in rooms:
        for label, lump in lumps:
            directory += struct.pack("<ii8s", len(data) + 12, len(lump), name(label))
            data += lump
    path.write_bytes(b"PWAD" + struct.pack("<ii", len(directory) // 16, len(data) + 12) + data + directory)


if __name__ == "__main__":
    mode = sys.argv[2:]
    gallery = horror_rooms if "--horror" in mode else final_rooms if "--finish" in mode else initial_rooms
    write_gallery(Path(sys.argv[1]), gallery())

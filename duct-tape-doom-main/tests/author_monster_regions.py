#!/usr/bin/env python3
"""Generate additional stock fingerprints and reviewed Arch-vile head caps.

Usage: python tests/author_monster_regions.py DOOM2.WAD platform/monster_regions_stock.h
No sprite pixels are redistributed. G/H/I and L/M use small reviewed skull
caps in sprite-origin coordinates. J/K/N/O/P obscure the skull with spell
art and deliberately have no critical head. Other live views use the
conservative stock anatomy recipes, including raise/heal animations.
"""
import re
import struct
import sys
from pathlib import Path


def generate(iwad, destination):
    data = Path(iwad).read_bytes()
    count, directory = struct.unpack_from('<ii', data, 4)
    lumps = {}
    for i in range(count):
        offset, size, name = struct.unpack_from('<ii8s', data, directory + 16*i)
        lumps[name.rstrip(b'\0').decode('ascii')] = data[offset:offset+size]
    root = Path(__file__).resolve().parents[1]
    source = (root / 'linuxdoom-1.10/info.c').read_text()
    families = {'TROO', 'SKEL', 'VILE', 'BOS2', 'BOSS', 'FATT', 'CYBR', 'SARG', 'HEAD', 'PAIN', 'BSPI', 'SPID'}
    frames = {family: set() for family in families}
    for family, number, state in re.findall(r'\{SPR_(\w+),(\d+),[^\n]+// (S_\w+)', source):
        if family in families and not any(part in state for part in ('DIE', 'RAISE', 'S_NULL')):
            frames[family].add(chr(65+(int(number) & 0x7fff)))
    entries = []
    for name, patch in sorted(lumps.items()):
        if len(name)<6 or name[:4] not in families:
            continue
        if name[4] not in frames[name[:4]] and not (len(name)==8 and name[6] in frames[name[:4]]):
            continue
        width, height, left, top = struct.unpack_from('<hhhh', patch)
        pixels = bytearray(width*height*2)
        for x in range(width):
            pos = struct.unpack_from('<I', patch, 8+4*x)[0]
            previous = -1
            while patch[pos] != 255:
                row, length = patch[pos], patch[pos+1]
                if row <= previous:
                    row += previous
                previous = row
                for y in range(length):
                    if row+y < height:
                        index = ((row+y)*width+x)*2
                        pixels[index:index+2] = bytes((patch[pos+3+y], 255))
                pos += length+4
        fingerprint = 2166136261
        for value in pixels:
            fingerprint = ((fingerprint ^ value)*16777619) & 0xffffffff
        mode, box = 2, (0, 0, 0, 0)
        if name[:4]=='VILE' and name[4] in 'GHIJKLMNOP':
            if name[4] in 'GHI':
                mode, box = 1, (left-5, top-66, left+5, top-53)
            elif name[4] in 'LM':
                mode, box = 1, (left-7, top-57, left+3, top-46)
            else:
                mode = 0
        escaped_name = name.replace('\\', '\\\\').replace('"', '\\"')
        entries.append(f'    {{"{escaped_name}",{width},{height},0x{fingerprint:08x}u,{mode},{box[0]},{box[1]},{box[2]},{box[3]}}},')
    Path(destination).write_text('''// Generated identity/landmark metadata; contains no commercial artwork.
// Regenerate with tests/author_monster_regions.py and an owned Doom II IWAD.
#ifndef DOOM_MONSTER_REGIONS_STOCK_H
#define DOOM_MONSTER_REGIONS_STOCK_H
struct doom_region_stock_entry {const char *name;int width,height;unsigned hash;int mode,x0,y0,x1,y1;};
static const doom_region_stock_entry doom_region_stock_table[]={
'''+'\n'.join(entries)+'\n};\n#endif\n')
    print(f'{len(entries)} stock frame fingerprints written')


if __name__ == '__main__':
    generate(sys.argv[1], sys.argv[2])

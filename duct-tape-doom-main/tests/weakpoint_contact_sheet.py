#!/usr/bin/env python3
"""Compose the critical smoke test's overlays/captures (requires Pillow).
Usage: python tests/weakpoint_contact_sheet.py ../build/critical-smoke
"""
from pathlib import Path
import sys
from PIL import Image, ImageDraw

NAMES={3:'Arch-vile',5:'Revenant',8:'Mancubus',11:'Imp',12:'Pinky',13:'Spectre',14:'Cacodemon',15:'Baron',17:'Hell Knight',19:'Spider Mastermind',20:'Arachnotron',21:'Cyberdemon',22:'Pain Elemental'}


def sheet(paths, destination, columns, width, height, overlay=False):
    if not paths:
        return
    canvas=Image.new('RGB',(columns*width,((len(paths)+columns-1)//columns)*height),(22,25,25))
    draw=ImageDraw.Draw(canvas)
    for index,path in enumerate(paths):
        image=Image.open(path).convert('RGB')
        if overlay:
            scale=max(1,min(3,(width-8)//image.width,(height-28)//image.height))
            image=image.resize((image.width*scale,image.height*scale),Image.Resampling.NEAREST)
        else:
            image.thumbnail((width-8,height-28))
        x=index%columns*width;y=index//columns*height
        canvas.paste(image,(x,y+24));parts=path.stem.split('-')
        label=NAMES.get(int(parts[1]),parts[1])+' '+('-'.join(parts[2:]))
        draw.text((x+3,y+3),label,fill='white')
    canvas.save(destination)
    print(destination)


if __name__=='__main__':
    root=Path(sys.argv[1])
    overlays=sorted((p for p in root.glob('regions-*.ppm') if len(p.stem.split('-'))==4),key=lambda p:tuple(map(int,p.stem.split('-')[1:])))
    sheet(overlays,root/'region-overlays.png',6,220,220,True)
    captures=sorted(root.glob('enhanced-*-*.png'),key=lambda p:(int(p.stem.split('-')[1]),p.stem.endswith('after')))
    sheet(captures,root/'enhanced-captures.png',4,328,228)

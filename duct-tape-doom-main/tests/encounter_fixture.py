"""Generate a tiny, non-copyrighted director fixture using the existing arena builder."""
from pathlib import Path
import sys,struct
from ragdoll_arena import build,name8
path=Path(sys.argv[1]);path.parent.mkdir(parents=True,exist_ok=True)
rects=[(-256,-400,256,400,0,256),(1200,-400,1900,400,0,256)]
lumps=build(rects,'imp',row=(),facings=(),player=0)
things=[(0,0,0,1,7),(1800,-220,180,16,15),(1800,220,180,16,15),(1800,0,180,66,15)]
for i,kind in enumerate([3004,9,3001,3004,9,3001]):things.append((1400+(i//3)*200,-250+(i%3)*200,180,kind,15))
for i,kind in enumerate([2007,2048,2007,2048,2011,2012,2011,2012,2001,82,2001,82,5]):
 things.append((-160+(i%4)*100,-300+(i//4)*180,90,kind,7))
lumps=[('MAP01',b'')]+[(name,b''.join(struct.pack('<5h',*t)for t in things)if name=='THINGS'else data)for name,data in lumps]
body=bytearray();directory=bytearray()
for name,data in lumps:directory+=struct.pack('<ii',12+len(body),len(data))+name8(name);body+=data
path.write_bytes(b'PWAD'+struct.pack('<ii',len(lumps),12+len(body))+body+directory)

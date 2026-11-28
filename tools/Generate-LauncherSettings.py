"""Regenerate the checked-in launcher catalogue from native F4 declarations (Python 3)."""
from pathlib import Path
import argparse, re, hashlib, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'duct-tape-doom-main/platform/settings_panel.cpp').read_text(encoding='utf-8')
def args(start):
    out=[]; begin=start; depth=0; quoted=False; escape=False
    for i in range(start,len(s)):
        c=s[i]
        if quoted:
            if escape: escape=False
            elif c=='\\': escape=True
            elif c=='"': quoted=False
        elif c=='"': quoted=True
        elif c in '({[': depth+=1
        elif c in ')}]':
            if c==')' and depth==0:
                out.append(s[begin:i].strip()); return out
            depth-=1
        elif c==',' and depth==0: out.append(s[begin:i].strip()); begin=i+1
    raise ValueError('Unterminated declaration')
def strings(v): return re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"',v)
root=ET.Element('settings', source='platform/settings_panel.cpp', source_sha256=hashlib.sha256((ROOT/'duct-tape-doom-main/platform/settings_panel.cpp').read_bytes()).hexdigest())
markers=list(re.finditer(r'^        \{"([^"\n]+)",\{|tabs\.push_back\(\{"([^"\n]+)"',s,re.M))
seen=set()
for m in re.finditer(r'\b(check|choice|slider|key)\(\s*"',s):
    a=args(m.end()-1)
    if len(a)<3: continue
    key=strings(a[0])[0]
    if key in seen: continue
    seen.add(key)
    prior=[x for x in markers if x.start()<m.start()]
    category=next((g for g in prior[-1].groups() if g),'Advanced') if prior else 'Advanced'
    if category=='Effects': category='Destruction'
    if key in {'encounter_random_seed','encounter_max_local_threat','encounter_max_spawn_attempts','encounter_debug'}: category='ED Advanced'
    if key in {'corrosive_barrels','acid_splatters','acid_monster_coating','incendiary_barrels'} or key.startswith('wp_') or any(x in key for x in ['critical','bleed','frenzy','feeding','weakpoint','eye_injur','elemental','monster_skin']): category='Combat'
    attrs=dict(key=key,title=' '.join(strings(a[1])),category=category,kind=m.group(1))
    if m.group(1)=='check': attrs['choices']='0=Off|1=On'
    if m.group(1)=='choice' and len(a)>5:
        labels=strings(a[3]); vals=re.findall(r'-?\d+',a[4])
        if len(labels)==len(vals): attrs['choices']='|'.join(v+'='+l for v,l in zip(vals,labels))
    if m.group(1)=='slider' and len(a)>6:
        for label,v in zip(['min','max','step'],a[3:6]):
            if re.fullmatch(r'-?(?:\d+(?:\.\d*)?|\.\d+)',v): attrs[label]=v
    ET.SubElement(root,'option',attrs)
# The director builds its threat controls in a loop rather than literal calls.
for key in re.findall(r'"(encounter_threat_[a-z_]+)"',s):
    if key in seen: continue
    seen.add(key)
    ET.SubElement(root,'option',key=key,title=key.removeprefix('encounter_threat_').replace('_',' ').title()+' threat',category='ED Advanced',kind='slider',min='1',max='128',step='1')
# Weak-point balance is persisted separately from the F4 option registry.
weak=(ROOT/'duct-tape-doom-main/linuxdoom-1.10/p_weakpoints.cpp').read_text(encoding='utf-8')
for key,low,high in re.findall(r'WP_KEY\(\w+,"([a-z_0-9]+)",(-?\d+),(-?\d+)\)',weak):
    key='wp_'+key
    if key in seen: continue
    seen.add(key)
    ET.SubElement(root,'option',key=key,title=key[3:].replace('_',' ').title(),category='Combat tuning',kind='slider',min=low,max=high,step='1')
ET.indent(root)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=ROOT/'launcher/settings-schema.xml')
ET.ElementTree(root).write(parser.parse_args().output,encoding='utf-8',xml_declaration=True)
print(f'Generated {len(seen)} F4 and balance setting descriptors')

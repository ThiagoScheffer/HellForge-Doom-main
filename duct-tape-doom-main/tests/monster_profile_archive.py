"""Validate or atomically create the source snapshot named by a run manifest."""
import argparse,hashlib,json,os,tempfile,zipfile
from pathlib import Path

def sha(data):return hashlib.sha256(data).hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('manifest',type=Path);p.add_argument('archive',type=Path)
    a=p.parse_args();manifest=json.loads(a.manifest.read_text());root=Path(__file__).resolve().parents[2]
    expected={name.replace('\\','/'):value for name,value in manifest['source_sha256'].items()}
    if a.archive.exists():
        with zipfile.ZipFile(a.archive) as archive:
            for name,value in expected.items():
                if sha(archive.read(name))!=value:raise RuntimeError('Archive mismatch: '+name)
        print('Verified existing source snapshot:',a.archive);return
    contents={name:(root/name).read_bytes() for name in expected}
    for name,value in contents.items():
        if sha(value)!=expected[name]:raise RuntimeError('Working source changed since measurement: '+name)
    a.archive.parent.mkdir(parents=True,exist_ok=True)
    handle,temporary=tempfile.mkstemp(dir=a.archive.parent,suffix='.zip.tmp');os.close(handle)
    try:
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED) as archive:
            for name,value in contents.items():archive.writestr(name,value)
            archive.writestr('measurement-manifest.json',json.dumps(manifest,indent=2))
        os.replace(temporary,a.archive)
    finally:
        if os.path.exists(temporary):os.unlink(temporary)
    print('Created source snapshot:',a.archive)

if __name__=='__main__':main()

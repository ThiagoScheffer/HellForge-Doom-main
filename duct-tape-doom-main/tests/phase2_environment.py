"""Export local diagnostic provenance/diff; never edits source or commercial resources."""
from pathlib import Path
import hashlib,json,platform,shutil,subprocess,datetime,re

root=Path(__file__).resolve().parents[2]
out=root/"build"/"phase2-delivery";out.mkdir(parents=True,exist_ok=True)
def git(*args):return subprocess.check_output(["git",*args],cwd=root)
metadata={"commit":git("rev-parse","HEAD").decode().strip(),"branch":git("branch","--show-current").decode().strip(),
          "status":git("status","--short").decode(),"platform":platform.platform(),"cpu":platform.processor(),
          "updated_utc":datetime.datetime.now(datetime.timezone.utc).isoformat(),"configurations":{}}
for variant in ('profile-off','profile-on','profile-args','native-off','native-on','engine-profile'):
    build=root/'build'/variant
    cache={}
    for line in (build/'CMakeCache.txt').read_text().splitlines():
        if line.startswith(('CMAKE_','DOOM_','BUILD_TESTING:','FETCHCONTENT_SOURCE_DIR_')) and '=' in line:
            key,value=line.split('=',1);cache[key]=value
    compilers={}
    for source in (build/'CMakeFiles').glob('*/CMake*Compiler.cmake'):
        for key,value in re.findall(r'set\((CMAKE_(?:C|CXX)_COMPILER(?:_VERSION|_ID)?) "([^"\n]+)"\)',source.read_text()):compilers[key]=value
    metadata['configurations'][variant]={'cache':cache,'compilers':compilers}
    cmake=next((value for key,value in cache.items() if key.startswith('CMAKE_COMMAND:')),None)
    if cmake:metadata['cmake_version']=subprocess.check_output([cmake,'--version'],text=True).splitlines()[0]
command="$cpu=Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors; $gpu=Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion; $os=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,TotalVisibleMemorySize; @{cpu=$cpu;gpu=$gpu;os=$os} | ConvertTo-Json -Depth 4"
try:metadata["hardware"]=json.loads(subprocess.check_output(["powershell","-NoProfile","-Command",command],text=True))
except Exception as error:metadata["hardware_error"]=str(error)
patch=git("diff","--binary")
for name in git("ls-files","--others","--exclude-standard").decode().splitlines():
    if not name.startswith(("duct-tape-doom-main/","docs/")):continue
    result=subprocess.run(["git","diff","--no-index","--binary","--","/dev/null",name],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if result.returncode not in (0,1):raise RuntimeError(result.stderr.decode())
    patch+=result.stdout
(out/"phase2.patch").write_bytes(patch)
metadata["diff_sha256"]=hashlib.sha256(patch).hexdigest()
(out/"environment.json").write_text(json.dumps(metadata,indent=2))
for mode in ("on","off"):
    source=root/"build"/("profile-"+mode)
    destination=root/"build"/"final-classic"/"binaries";destination.mkdir(exist_ok=True)
    if not (destination/(mode+".exe")).exists():shutil.copy2(source/"Release"/"doom-headless.exe",destination/(mode+".exe"))
    if not (destination/(mode+"-CMakeCache.txt")).exists():shutil.copy2(source/"CMakeCache.txt",destination/(mode+"-CMakeCache.txt"))
print("Exported",out)

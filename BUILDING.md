# Building on Windows

Install Visual Studio 2022 C++ Build Tools with CMake support and the Windows SDK. The launcher uses the .NET Framework C# compiler included with Windows.

Install Python 3.9+ with `python` or `py` available on PATH. Every launcher
build, including `-LauncherOnly`, regenerates its settings catalogue from the
native F4 settings and weak-point declarations before embedding it. Regular
builds write `launcher/settings-schema.xml`. The `-MonsterProfiling` build
writes its catalogue to `build/monster-profiling/settings-schema.xml`.
Generation failures stop the build. Full native builds also refresh the
`platform/settings_panel.cpp` timestamp to force its compilation during
incremental rebuilds. Launcher-only builds update the catalogue but retain
the existing engine; use a full build to apply native F4 code changes.

## One-click Windows builds

Close the game and launcher, then double-click a batch file in the repository root:

| Batch file | Result |
| --- | --- |
| `Build-Hellforge.bat` | Builds the native project and embeds the engine in `dist/DOOM-Hellforge.exe` using the existing release script. |
| `Build-Hellforge-V2.bat` | Forces a clean rebuild of all native code and dependencies, then rebuilds the launcher. Use when copied/restored source timestamps are older than existing objects or binaries. |
| `Recompile-Shaders.bat` | Regenerates `platform/gpu_shaders.h` with all Vulkan SPIR-V and Metal MSL variants. Run the build batch afterward to embed them. |

The windows remain open to show success/errors. Logs are saved under
`build/one-click/`. These batches do not run game tests or change the release version.
Normal builds use the checked-in shader header and do not require shader tools.

V2 always uses CMake `--clean-first`; it does not rely on detecting modified file
timestamps. It cleans build products, preserves source/settings and the CMake
cache, and takes longer than the incremental build. Close the game and launcher
first. A failed native rebuild stops before repackaging the launcher. V2 does not
change the release version or run tests. Shader source compilation remains a
separate step unless `-RecompileShaders` is supplied. `-LauncherOnly` is rejected
by V2 because it would skip the promised native rebuild.

Shader recompilation requires **glslangValidator.exe** and **spirv-cross.exe**.
The helper searches PATH, `VULKAN_SDK\Bin`, installed `C:\VulkanSDK` folders and
`tools/shader-tools/`. Alternatively set `HELLFORGE_GLSLANG` and
`HELLFORGE_SPIRV_CROSS` to their full executable paths. Failed shader compilation
preserves the existing header. No tools are installed or downloaded automatically.
The locally required `launcher/DiggingMaterials.wad` and bundled mod archives
must be present before packaging; a commercial game IWAD is not needed to build.

Optional commands from the repository root:

```powershell
.\Build-Hellforge.bat -LauncherOnly       # Repackage the existing engine
.\Build-Hellforge.bat -RecompileShaders   # Shaders, native build, then launcher
.\Build-Hellforge.bat -CheckOnly          # Check dependencies without compiling
.\Build-Hellforge-V2.bat                 # Clean native rebuild, then launcher
.\Build-Hellforge-V2.bat -RecompileShaders # Shader regeneration, clean native rebuild, launcher
.\Build-Hellforge-V2.bat -CheckOnly      # Check V2 prerequisites; no cleaning/building
.\Recompile-Shaders.bat -CheckOnly       # Check shader tools without generating
```

The PowerShell implementations are under `tools/`. CMake can be selected for
the shader helper using `HELLFORGE_CMAKE`; the full build retains the existing
launcher script's Visual Studio/CMake selection and configuration.

## Command-line builds

From the repository root, run:

```powershell
powershell -ExecutionPolicy Bypass -File launcher/build.ps1
```

The resulting launcher is `dist/DOOM-Hellforge.exe`. The script embeds the engine, launcher image, graphics defaults, crater material and the three mod archives. Vendored dependency sources are in `dependencies/`. No commercial IWAD is supplied.

Set the four-part release version in `launcher/VERSION` before building. The script embeds that version, a UTC build timestamp and the engine SHA-256 prefix into the executable. To rebuild only the launcher using the existing `build/Release/doom.exe`, run `powershell -ExecutionPolicy Bypass -File launcher/build.ps1 -LauncherOnly`.

If the normal output executable is running, add `-OutputPath dist/DOOM-Hellforge-updated.exe` to build a separate copy. Settings backup regression tests can be compiled and run from the repository root after building:

```powershell
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe /main:SettingsBackupTests /out:build\SettingsBackupTests.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Xml.Linq.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /resource:launcher\hero.png,hero.png launcher\Launcher.cs launcher\LanMenu.cs launcher\SettingsBackupTests.cs build\LauncherVersion.cs
& .\build\SettingsBackupTests.exe
```

To enable gameplay/GPU checks, configure with your own IWAD:

```powershell
cmake -S duct-tape-doom-main -B build -G "Visual Studio 17 2022" -A x64 -DDOOM_BUILD_SDL=ON -DDOOM_RENDER3D=ON -DBUILD_TESTING=ON -DDOOM_TEST_GPU=ON -DDOOM_GAME_DATA="C:/Games/Doom/DOOM.WAD"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

GPU tests create game windows. The original engine documentation is preserved in `duct-tape-doom-main/README.md`; the root README describes this customized build. The weapon-switch capture data and its regeneration procedure are documented in `duct-tape-doom-main/tools/switch-reference/README.txt`.

Shared destruction geometry tests: `cmake -S . -B volume-build`, `cmake --build volume-build --config Release`, then `ctest --test-dir volume-build -C Release --output-on-failure`.
Large-map compatibility regressions use an owned Doom II IWAD. Configure
`-DDOOM_COMPAT_TEST_IWAD="D:/Games/DOOM2.WAD"` and optionally
`-DDOOM_COMPAT_TEST_WAD="F:/Maps/Sunder 2512.wad"`, then build and run CTest as
above. These add generated format/action tests and an isolated per-map loading,
classic-frame and save/load check. See
[the compatibility report](docs/changes/BIG-WAD-COMPATIBILITY.md) for commands and
the distinction between smoke coverage and campaign completion.

Reload stability and persistent map-lighting cache checks use an original Doom II IWAD.
Run the same command with the original Doom IWAD in a separate output directory to exercise E1M1 restart.

```powershell
cmake --build build --config Release --target doom doom-reload-smoke --parallel 8
python duct-tape-doom-main/tests/reload_cache.py --iwad test-references/DOOM2.WAD --engine build/Release/doom-reload-smoke.exe --out build/reload-release-verification --cycles 10 --samples 5
ctest --test-dir build -C Release -E "^bigmap-" --output-on-failure
```

Use `-verifybakecache` with an isolated cache to compare restored channels to a reference bake.
Keep `build/Release/doom.pdb` with the matching engine build for crash analysis. See [reload/cache details](docs/changes/RELOAD-LIGHTING-CACHE.md).

## Performance Hotspot Tracer

Open **F4 → Performance → Diagnostics**, enable **Performance Tracer**, and
apply the settings. Set Target FPS to match the pacing you expect. Automatic
export writes completed incidents locally. **Export Performance Report** saves
retained incidents and recent frames, including any incident in progress.
Reports are in `Diagnostics/Performance` under the game's working directory,
normally the Hellforge settings and save folder.

The tracer records CPU timings, swapchain waits, visibility and light-mask
workload. Detailed capture retains incident light evidence and samples focused
materials. See the [tracer schema and diagnostics](docs/changes/PERFORMANCE-HOTSPOT-TRACER.md).
GPU execution time is unavailable in the bundled SDL GPU API. Run the focused synthetic test with
`ctest --test-dir build -C Release -R "^performance-trace$" --output-on-failure`.

## Monster performance diagnostics and current controls

The normal launcher build explicitly sets `DOOM_MONSTER_PROFILING=OFF`,
`DOOM_MONSTER_PROFILE_ARGUMENTS=OFF` and `DOOM_MONSTER_BENCHMARK=OFF`, including
when reusing a build directory previously used for diagnostics. Normal F4
controls still include ambush sight reuse and conservative sprite filtering,
both default off. Gameplay also exposes restricted classic-only idle-search
reuse and aggregate swarm diagnostics.

Build the launcher with the live monster profiler enabled using this command.

```powershell
powershell -ExecutionPolicy Bypass -File launcher/build.ps1 -MonsterProfiling
```

Open `build/monster-profiling/launcher/DOOM-Hellforge-Monster-Profiling.exe`,
select the IWAD and map WAD, and enter
`-profilemonsters "Diagnostics\Hellforge Monsters"` in **Engine Arguments**.
Use a writable directory and a separate directory per recording. Play the
problem area, then exit the game to flush the results. The launcher forwards
the arguments and keeps the engine output in **Open Last Log**. Relative paths
resolve from the Hellforge settings and save folder.

For fixed-tic benchmark drivers, use a separate build directory, configure
`-DDOOM_MONSTER_PROFILING=ON -DDOOM_MONSTER_BENCHMARK=ON`, then build
`doom-monster-bench` and/or `doom-headless`. Runtime profiling needs
`-profilemonsters <output-directory>`; wake auditing additionally needs
`-bench-wake-audit`. Use owned WADs and isolated configs. See the
[profiling report](docs/changes/MONSTER-PROFILING-PHASE2.md) and
[MAP02 exact-reuse checks](docs/changes/AMBUSH-SIGHT-REUSE.md). Some historical
runner scripts assume the sibling experimental worktree or locally supplied
WADs; adjust their paths before use. They are not part of the normal launcher.

Current integration and focused acceptance checks:
[2026.11.14.1](docs/changes/MAIN-INTEGRATION-2026-11-14.md).

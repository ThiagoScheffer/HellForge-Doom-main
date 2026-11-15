# Building on Windows

Install Visual Studio 2022 C++ Build Tools with CMake support and the Windows SDK. The launcher uses the .NET Framework C# compiler included with Windows.

## One-click Windows builds

Close the game and launcher, then double-click a batch file in the repository root:

| Batch file | Result |
| --- | --- |
| `Build-Hellforge.bat` | Builds the native project and embeds the engine in `dist/DOOM-Hellforge.exe` using the existing release script. |
| `Recompile-Shaders.bat` | Regenerates `platform/gpu_shaders.h` with all Vulkan SPIR-V and Metal MSL variants. Run the build batch afterward to embed them. |

The windows remain open to show success/errors. Logs are saved under
`build/one-click/`. Neither batch runs game tests or changes the release version.
Normal builds use the checked-in shader header and do not require shader tools.

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

Reload stability and persistent map-lighting cache checks use only original Doom II:

```powershell
cmake --build build --config Release --target doom doom-reload-smoke --parallel 8
python duct-tape-doom-main/tests/reload_cache.py --iwad test-references/DOOM2.WAD --engine build/Release/doom-reload-smoke.exe --out build/reload-release-verification --cycles 10
ctest --test-dir build -C Release -E "^bigmap-" --output-on-failure
```

Keep `build/Release/doom.pdb` with the matching engine build for crash analysis. See [reload/cache details](docs/changes/RELOAD-LIGHTING-CACHE.md).

## Monster performance diagnostics and current controls

The normal launcher build explicitly sets `DOOM_MONSTER_PROFILING=OFF`,
`DOOM_MONSTER_PROFILE_ARGUMENTS=OFF` and `DOOM_MONSTER_BENCHMARK=OFF`, including
when reusing a build directory previously used for diagnostics. Normal F4
controls still include ambush sight reuse and conservative sprite filtering,
both default off. Gameplay also exposes restricted classic-only idle-search
reuse and aggregate swarm diagnostics.

For diagnostic work, use a separate build directory, configure
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

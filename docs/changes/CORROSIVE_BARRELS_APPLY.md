# Applying Corrosive Barrels v2

This package applies to HELLFORGE revision `06c786b614ff403ff43e68034396ce702f0f8b32` with Corrosive Barrels v1 already present. The archive provides the changed files at their repository paths and `HELLFORGE_Corrosive_Barrels_v2.patch`.

1. Extract the archive outside the working tree and review the [audit](CORROSIVE_BARRELS_AUDIT.md) and [test report](CORROSIVE_BARRELS_TEST_REPORT.md).
2. From the checkout root, run `git apply --check <path-to-patch>`. If it passes, run `git apply <path-to-patch>`. Use either the patch or the file overlay.
3. Build with `Build-Hellforge.bat` and enable **F4 → Combat → Corrosive Barrels** to try the feature. The generated shader header is included. `Recompile-Shaders.bat` regenerates it from the shader sources.
4. Keep a copy of any save with active acid if reverting to the older engine. Loading with v2 and the setting off discards acid state, and the next save omits the trailer.

The archive contains source and reports. Build output, save files and WAD assets are omitted.

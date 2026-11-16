---
build: "2026.11.15.6 (local native rebuild)"
updated_utc: "2026-11-16T05:27:02.923818+00:00"
status: "implemented; focused navigation/painter checks passed"
feature: "ENCOUNTER-DIRECTOR.md"
tests: "../../duct-tape-doom-main/tests/settings_navigation.cpp"
index: "../README.md"
---

# F4 Encounter Director tabs and text overlap

F4 now shows one **ENCOUNTER DIRECTOR** sidebar entry. The five ED pages are
inside it as clickable tabs: **ED Monsters**, **ED Reinforcements**, **ED Items**
**ED Population** and **ED Advanced**. Their widths use the real font metrics so the standard Doom
labels remain readable; the internal tab strip omits the repeated ED prefix. Overview contains the master and four module switches.

- Left/Right on the **Page** selector selects Overview or a child page, using
  the existing keyboard/controller control path.
- **Q/E** cycles director pages. Clicking the sidebar parent returns to Overview.
- **Tab / Shift+Tab / Page Up / Page Down** still select main categories.
- Esc/F4 applies, F10 discards, and director reset changes only the draft.

The previous sidebar drew every category without a row limit. Its last entries
therefore painted across the description rows. It now draws only the 17 available
rows, scrolls to the selected category, translates clicks through that scroll
offset, and displays overflow arrows. Header/section text is clipped to its
allocated width. End selects an actual option instead of a trailing information
heading, avoiding a null selected-option dereference on the ED pages.

Grouping is presentation-only in `platform/settings_menu.cpp`: the flat option
registry remains intact, preserving configuration keys, defaults, import/export,
settings persistence and gameplay behavior. The original nesting fix changed no actor behavior; the subsequent
[Population module](ORIGINAL-MONSTER-STREAMING.md) is an independent gameplay experiment.

## Focused verification

`doom-settings-navigation-tests` drives the real F4 handlers and CPU painter with
fonts loaded from the original Doom II IWAD. It verifies nesting, all five pages,
Q/E wrap, normal choice navigation, mouse tab switching, End, sidebar scrolling
and clicks, F4 apply, F10 discard, and reset scope. Adding 25 synthetic categories
leaves the entire description band pixel-identical, directly checking the overlap.

Three 640x400 menu captures were generated and inspected: Overview,
Reinforcements and Advanced. These are actual CPU menu renders, not GPU/full-game
captures. GPU capability queries/settings-apply output are stubbed in this focused
fixture; no full render or physical-controller test is claimed. The headless audio
backend emits existing sample-cache notices during menu sounds; the test exits 0.

Build and run only the relevant target, using an isolated writable directory:

```powershell
cmake --build build --config Release --target doom doom-settings-navigation-tests
build/Release/doom-settings-navigation-tests.exe <path-to-DOOM2.WAD>
```

Supplying `DOOM_COMPAT_TEST_IWAD` also registers CTest `f4-director-navigation`.
Local evidence is under `build/f4-director-tabs/`, including `navigation.log` and
`director-overview.png`, `director-reinforcements.png`, `director-advanced.png`.
The native engine was rebuilt; the launcher and distribution were not packaged.
The previous `build/encounter-director/Play-Encounter-Director.cmd` uses this rebuilt
engine and its isolated profile.

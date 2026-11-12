# HELLFORGE launcher theme and repaint fix

Build: 2026.11.12.1 (launcher-only rebuild, 12 November 2026 NZ time).

The launcher and LAN dialog now use charcoal backgrounds, copper borders,
ember-orange actions, amber hover highlights, and warm text. Existing artwork
is retained. The local user modification to hero.png was preserved and embedded.

ModernButton now owns background painting and fills its entire client area
with the opaque parent background before drawing rounded edges and text.
Primary keyboard focus uses a dark cue; other buttons use amber.
Parent background changes invalidate the button, and event subscriptions are
removed on disposal.

Verification:

- 54 painting checks pass across primary, ordinary and quiet buttons at
  1x, 1.5x and 2x synthetic sizes. Thirty hover/unhover cycles per variant
  restore every resting pixel, including corners.
- Disabled/enabled, simulated drag-out release, resize restoration and
  changed-parent backgrounds pass.
- 18 launcher persistence checks and 17 settings/backup checks pass.
- Launcher and LAN captures inspected; minimum-size layout capture retained.
- Engine binary and release version retained; launcher rebuilt with LauncherOnly.

Evidence: build/launcher-paint/ contains launcher, LAN and per-button PNGs.
launcher/LauncherPaintTests.cs reproduces pixel checks using repeated paints
onto the same bitmap rather than a clean bitmap for every state.

Limitations: Control.Scale captures are synthetic and do not certify real
monitor DPI transitions. Interactive keyboard focus navigation and physical
window occlusion were not automated. The inherited fixed launcher layout
repositions some controls during synthetic scaling, so those captures are not
a claim of full high-DPI layout support.

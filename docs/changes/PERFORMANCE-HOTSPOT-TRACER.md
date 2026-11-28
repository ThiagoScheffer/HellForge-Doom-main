# Performance Hotspot Tracer

Open **F4 → Performance → Diagnostics** to enable the local trace. Basic
captures frame pacing, CPU stages, actual world visibility state and mask
workload. Detailed adds reserved light evidence and sampled focused and
material attribution. Existing target, duration, capture mode and export
settings apply when the panel closes.

## Incidents and coverage

An incident has one stable ID within a trace session. Camera and sector
changes stay within the incident. A child segment uses 64-unit position
bins, 15-degree yaw and pitch bins, sector, subsector and map instance. A
new grouping must persist 100 ms. Yaw is circular and still matters at
steep pitch because screen orientation can change the view. Exact pose is
retained independently in frame rows.

The detector uses monotonic elapsed time. Entry starts at a frame slower
than 90% of target, and a 150 ms time-weighted filter bridges short pacing
variations. Confirmation requires multiple samples, the configured span and
a slow current frame. Spike and severe thresholds are two and three times
the target frame budget. An active parent absorbs subsequent spikes.
Recovery uses the distinct 95% threshold for 500 ms. An interrupted
recovery returns to the parent active window. After confirmed recovery,
up to three seconds of context are retained. A new degraded period closes
that context and begins a new candidate. Map/load, settings, presentation,
resolution, resource reconstruction, focus and panel transitions partition
eligible timing. Incidents include an explicit termination reason.

A local schema 1 LOWFPS report had consecutive incidents 4 and 5 at frame
serials 3905 and 3906 in the same settings revision and timing segment.
Neither incident retained a frame at or below 17.544 ms. Incident 19
contained 48 such frames among 63 retained active-window rows. Short
incident duration alone could also result from a settings or focus boundary.
The schema 2 report separates active, degraded, recovery and post-capture
statistics. FPS uses count divided by summed interval. Percentiles use the
retained sample population. The peak and trigger are retained independently
of the ordinary sample stores.

The history ring retains 2,048 rows. Each of up to 32 completed incidents
and one active incident has 144 earliest samples, a 256-row tail and
separately pinned trigger and peak frames. Up to 32 child segments are
recorded per incident. Coverage distinguishes circular history replacement,
pre/post context availability, intentional downsampling, incident eviction
and dropped exports. Prehistory can span up to five seconds within the same
timing segment. Re-exporting a session preserves original frame and incident
IDs. An export number identifies a copy.

Detailed capture stages copied light data from the renderer state belonging
to the interval-ending frame. Each incident reserves pre-incident, start,
peak and recovery slots, plus one separately labeled sampled-attribution
slot. There is no periodic snapshot queue competing for peak slots. A new
peak replaces its reserved slot. The engine keeps one pending or writing
export on its worker. The Windows x64 Release build accounts for 16,560,160
bytes for retained records, a full export copy, settings history and fixed
mask-accounting scratch. A compile-time 16 MiB bound covers these structures
and payloads. This is a diagnostic storage bound, excluding allocator and
transient serialization overhead.

## Renderer attribution

XY and 3D counts are the generated plane counts. The gate, camera validity
and effective XY fields separate a disabled path from a valid zero-plane
projection. Culling bypass bits are 1 explicit override, 2 liquid reflection,
4 glass setting, 8 planned glass capture, 16 pending bake, 32 loading,
64 global digging, 128 dig mesh, 256 geometry cuts, 512 floor bounds and
1024 wall bounds. The geometry path and shared-batch flag describe the
scope of the primary geometry preparation. The legacy visible-subsector
and visible-line fields count candidates. Acceptance means the XY test
retained the candidate. Emitted floor, ceiling and wall counters cover
ordinary BSP geometry. The aggregate world count also includes special
geometry and sky.

Basic mask counters come from the existing CPU assignment loop. The
per-job result is reduced on the owning thread, separately for main
batches and mirror extras. Secondary passes can reuse main batches.
Persistent section mask stamps determine upload reuse. CPU masks are
recomputed and included each frame. Serial verification passes perform
no diagnostic accounting. A light–triangle pair is one assigned bit in
either word on one triangle. These counts describe assignment workload,
including assignments to fully baked lights. They do not measure shaded
fragments or exact shader cost. Frames above 8,192 mask jobs leave these
diagnostic values empty.

Detailed mode samples at most once per 100 ms, using incident signals and
a periodic opportunity. The center geometry ray uses the final camera,
interpolated surfaces and solid walls. Larger maps and unsupported geometry
paths can leave the result unavailable. Transparent overlays, sprites,
sky and post-processing can alter the final pixel. Focus and material
records name the translated texture or flat and, when already loaded,
the compatible next animation frame and blend. Main material workload
shows up to 16 named, non-sprite batches ordered by processed triangles.
Detailed mask sampling is capped at 16,384 main triangles. The histogram
and per-light counts are omitted when the sample does not reconcile with
the recorded main totals. Provenance travels with selected surface lights.
A merged light retains its contributor count and common fields only.

## GPU timing capability and roadmap

The bundled and tested runtime is SDL **3.4.18**, using SDL_GPU with Vulkan.
Its public API exposes submission fences and fence polling. It does not
expose GPU timestamp writes, timestamp query results, valid timestamp bits
or timestamp periods. No live timestamp queries, extra fences or waits are
installed by the tracer. The separate benchmark retains its original
behavior. Schema 2 reports `unsupported_sdl_gpu_api` and a null duration.
Executed pass flags distinguish work that did run from passes that did not.

A future supported timestamp provider needs asynchronous query ownership
with device-generation and original-frame keys, nonblocking availability
checks, wrap-safe tick conversion, and a versioned late-result export.
Hardware timing and its associated pending-result tests are deferred until
the renderer has a supported timestamp API.

## Files and checks

The existing summary, session, settings, events, frame, hotspot and light
files remain. `incidents.json` adds parent and child records.
`attribution.jsonl` records evidence slots, availability, sampled surfaces,
materials and mask populations. Frame and light CSV extensions append
columns. Offline joins use session identity plus frame serial.

```powershell
cmake --build build/trace-link-verify --config Release --target doom doom-perf-smoke doom-performance-trace-tests doom-world-frustum-tests
ctest --test-dir build/trace-link-verify -C Release -R '^(performance-trace|world-frustum-bounds|sprite-frustum-bounds)$' --output-on-failure
```

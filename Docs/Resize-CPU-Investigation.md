# Resize CPU investigation — performance checkpoint after checkpoint 02

Validated checkpoint: `2de255d`, following the window-composition checkpoint. The intermediate samples and earlier failures below remain historical evidence.

The user reported about 4% process CPU, 2.3% GPU, and 30.4 MB memory while repeatedly dragging window edges. On 32 logical processors, a normalized 4% CPU reading corresponds to approximately 1.28 logical processors of scheduled CPU time. It does not establish where that time is spent or predict scaling on another machine. Functionality and presentation are constraints, not resources to exchange for performance.

## Measurement

`Scripts/resize_probe.py <executable-name>` runs 320 app-owned resize requests with 40 intermediate sizes in each direction, four cycles, no image readbacks. Seven adjacent requests repeat the same dimensions; the observed 313 presentations are not seven dropped geometry updates. Every request checks actual dimensions and completed revision. This synthetic workload complements, rather than replaces, mouse-drag/compositor validation.

Opt-in `RenderMetrics.h/.cpp` uses render-thread CPU cycles and performance-counter wall time. The resize stage includes its nested display-query stage: do not add both. `Present` wall time includes vsync waiting; cycles are not converted to milliseconds using an assumed processor frequency. Normal launches do not collect these stage timings.

First controlled samples on the existing 144 Hz host:

| Variant | Process CPU time | Workload wall time | EndDraw wall time |
| --- | ---: | ---: | ---: |
| Checkpoint 02 + diagnostics | 1,421.875 ms | 2,165.606 ms | 739.866 ms |
| Diagnostic only: omit text | 875.000 ms | 2,169.639 ms | 16.565 ms |
| Initial full-resolution GPU cache | 828.125 ms | 2,164.564 ms | 20.857 ms |

The initial cache sample used about 42% less process CPU than baseline. These are single samples of an intermediate candidate, not sustained performance certification of the final build. Text omission is an attribution experiment only, never a release behavior. Baseline text-layout creation consumed only 0.918 ms across this workload; the dominant measured text cost occurred during draw completion. This does not identify every internal DirectWrite/driver operation as CPU rasterization.

## Implementation

- `TextLayoutCache.h/.cpp` owns bounded shaped geometry. Unwrapped text is independent of viewport bounds; ellipsis width remains part of the key. It evicts least-recently-used entries instead of flushing every layout at 256 entries. Estimated shaped storage is limited to 8 MiB and 512 entries; the actual COM allocation size is opaque.
- `TextRasterCache.h/.cpp` owns up to 32 MiB of estimated GPU pixel storage and 512 entries, with a 16 MiB maximum individual image. Actual driver allocation overhead is additional. Each entry includes a strong layout reference, exact scale, fractional physical-pixel origin, and color. The cache is tied to its render-target resource domain and cleared for display/render-setting refresh, legacy target replacement, or device reconstruction.
- Compatible GPU targets retain the parent's SDR/HDR pixel format with premultiplied alpha. Text is drawn at destination DPI. Reuse maps one source pixel to one destination physical pixel; it does not stretch a window screenshot. Oversized text or an unavailable cache allocation is drawn directly through the existing hardware path, never omitted.
- Current geometry, text, selection, and UI state are drawn each frame. The existing synchronous `WM_NCCALCSIZE` transaction, exact-size swap chain, and composition commit remain in place. No resize input is deferred, no frame is intentionally skipped, and no update is frozen during dragging.
- Cache owners are noncopyable classes with public/private declarations in matching headers and out-of-line implementations. Their RAII destruction precedes font/device teardown. The broader application/document OOP refactor remains checkpoint 03; this is a bounded performance slice prompted by the subsequent CPU report.

## Validation status

Release and Debug app builds and model tests passed. Offscreen hardware tests compare cached and direct GPU drawing over 160 cases: SDR and FP16 HDR, 100/125/150/200% DPI and return to 100%, fractional origins, multiline and multilingual text, changed content/color, ellipsis, clipping, oversized uncached lines, cold/reused entries, eviction, and explicit clearing. Maximum allowed difference is one 8-bit channel level after HDR normalization. These tests do not open or manipulate a desktop window.

The SDR window probe passed at four widths/DPI settings, including three quiet two-second intervals with zero measured process CPU time and zero extra presentations. The no-readback interaction probe passed maximize/fullscreen/restore, rename fidelity, menus, and repeated shrink/expand. One earlier screenshot interaction run failed its rename-publication check; the no-readback repeat passed, but that earlier failure remains recorded rather than dismissed. A separate baseline screenshot run received mouse-wheel input and cannot serve as a pixel-identical whole-window reference.

Computer Use verified entering/exiting fullscreen through native window controls. Desktop capture failed twice with `SetIsBorderRequired: No such interface supported (0x80004002)`. The user then stopped Computer Use with Escape; desktop input stopped. In the following session the full interaction/readback and long-line window probes both passed. A final-binary resize sample used 640.625 ms process CPU over 2,167.696 ms for the same 320 requests and 313 presentations. This is consistent with the earlier improvement, but repeated controlled samples on additional hardware remain necessary before generalizing it. The user also reported that performance was looking very good. `Winip-dev-06.exe` is the validated performance checkpoint following `2e6fcdd`. The 160 Hz deadline and outside-window compositor behavior are not certified by offscreen pixel comparisons.

## Related work

[Cheng Lou's Pretext](https://github.com/chenglou/pretext) is the closest match found to the user's recollection, but it has not been identified conclusively as the academic paper. It is an MIT-licensed JavaScript/TypeScript library. Its [research log](https://github.com/chenglou/pretext/blob/main/RESEARCH.md) describes expensive segmentation/measurement once, followed by arithmetic over cached segment widths during resize. That separation is worth studying for future wrapped cells and rich Markdown.

Winip has no browser DOM and currently measures fixed-line notebook cells on its loader worker. Pretext would not directly replace the draw-completion work identified here. Preserve DirectWrite's shaping and Unicode behavior; do not substitute independent character-width sums for ligatures, bidi, combining marks, or script shaping. No third-party code or dependency was added.

Microsoft recommends reusing layouts and cached drawing resources in its [Direct2D performance guidance](https://learn.microsoft.com/en-us/windows/win32/direct2d/improving-direct2d-performance). Any future use remains subject to Winip's physical-resolution and presentation invariants.

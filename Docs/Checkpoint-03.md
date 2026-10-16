# Checkpoint 03 — first class-ownership slice

Status: bounded structural slice, built as `Winip-dev-07.exe` in Release and Debug. This follows documentation checkpoint `276e0ea` and preserves the reader/rename/discovery feature set. It does not complete the broader application refactor or add cell editing/kernel execution.

## Changes and guarantees

- `Notebook.h/.cpp` declares and implements an encapsulated value snapshot. Its cell list and original JSON are read-only to consumers. Copies and title replacements share immutable content; renaming no longer copies every cell and the complete JSON tree.
- Parsing constructs a complete snapshot before returning it. Parsed versus measured layout is explicit. `WithLayout` stages and validates all dimensions, computes cell positions and total extents using overflow-safe intermediate arithmetic, and returns a replacement. Invalid counts/dimensions or excessive height leave prior snapshots intact. DirectWrite preparation stays on the loader worker.
- `TitleEditor.h/.cpp` replaces the public header-only title buffer. Methods own activation, selection, insertion, deletion and caret movement. Positions stay inside the buffer and movement/deletion respect UTF-16 surrogate pairs. Replacing a selection uses one string replacement, preserving the previous state if allocation fails. Beginning an edit does not truncate an existing long filename.
- `FontSet.h/.cpp` now matches its class name. Its loader registration and private font collection have a noncopyable owner; the destructor retains the existing unregister/release lifecycle.
- Rendering and application callers use the public interfaces. GPU cache keys, full-resolution rasterization, DirectComposition resize transactions, and worker scheduling are unchanged.

Cell geometry still resides alongside parsed cell data. A layout replacement copies the cell vector on the loader worker; it must not become the future per-keystroke path. Source editing needs stable cell IDs, revision tracking and incremental updates. Title editing remains a bounded filename editor, not a complete grapheme/IME/clipboard implementation.

## Validation

Release and Debug app builds and test suites passed. Added model checks cover empty and measured states, shared snapshot storage, original-JSON preservation, rejected layout changes without partial mutation, and coherent following-cell positions. Title checks cover out-of-range/interior-surrogate positions, selection replacement, inactive mutation, long existing names and input limits. Existing malformed/large/Unicode notebook, rename collision, menu, interpreter and rendering-policy tests remain passing.

Each configuration passed all 160 hardware GPU cache/direct-render fidelity comparisons across SDR/HDR, DPI, fractional origins, clipping, changed content and cache lifecycle cases. The accepted tolerance remains one normalized 8-bit channel level.

App-owned Release probes passed title click/edit/cancel/apply, menu keyboard rename, byte-preserving file rename, maximize/fullscreen/restore, repeated shrink/expand, exact-size allocations and populated frame edges. Long-line HDR and SDR probes passed their four width/DPI phases, scrolling and restoration. Each recorded three quiet two-second intervals with zero measured process CPU time and zero additional presentations. Rendered images were inspected for title editing and repeated shrinking. Readback images cover application content, not the area outside the native window.

Computer Use initialization and app discovery succeeded, but launching the new build returned `Computer Use app approval timed out`. No external keyboard or desktop-capture verification was completed in this slice. The earlier user-confirmed outside-window artifact fix remains historical evidence; this checkpoint does not independently recertify it.

## Timing evidence and limits

Host: the Windows 10 / hardware GPU (model redacted) host from the earlier checkpoints; measured active refresh 144 Hz. The no-readback interaction run recorded maximum sampled ordinary UI work of 0.2941 ms, render preparation of 23.6403 ms, and native transition wall time of 44.5029 ms. These maxima do **not** satisfy or certify the active-refresh deadline. The image-readback run was intrusive, including a 1334.6651 ms maximum transition, and is retained as functional/image evidence only.

The resize workload requests 320 sizes, of which 313 change geometry. Every retained run presented all 313 changed sizes with matching physical allocation extents. Initial new/old samples were both slower than historical measurements: 1734.375/1828.125 ms CPU over roughly 5.4 seconds. Later samples returned to roughly 2.16 seconds wall time. No specific cause for that environmental variation was established.

Three additional alternating samples per build produced:

| Build | Process CPU samples (ms) | Workload wall-time range (ms) |
| --- | --- | --- |
| Previous `Winip-dev-06` | 1031.250, 890.625, 640.625 | 2163.923–2169.698 |
| Refactored `Winip-dev-07` | 890.625, 546.875, 843.750 | 2166.375–2168.537 |

These overlapping samples show no consistent slowdown attributable to the refactor; they do not establish a speedup or controlled performance equivalence. No functionality or presentation threshold was relaxed. Sustained frame deadlines, GPU timing, Windows 7–11, physical HDR and broader monitor/device-loss behavior remain open.

Reproduce with the commands in [Validation.md](Validation.md), using `--name Winip-dev-07`; use `python Scripts/resize_probe.py Winip-dev-06` for the prior comparison build. Raw evidence and the executable hash are under `validation/checkpoint-03-*.json`. Build identity is recorded in the manifest (local archive: `Artifacts/archive/validation/checkpoint-03-build.json`; not distributed); variable resize samples are retained in the comparison record (local archive: `Artifacts/archive/validation/checkpoint-03-resize-comparison.json`; not distributed).

## Next ownership work

Extract application/window/view orchestration, menu ownership, and loader/discovery worker lifetimes from the remaining global `State` and free functions. Preserve coherent document/path/title publication and constructor/destructor shutdown ordering. Keep this separate from the subsequent code/Markdown editing, insertion, safe saving and kernel slices in [Roadmap.md](Roadmap.md).

The current documentation map also retains [coding conventions](Coding-Guide.md), [UI direction](UI-Style-Guide.md), original visual samples and [Pretext/upstream references](References.md). Pretext is a research reference for future wrapping, not an adopted dependency or a replacement for native Unicode shaping.

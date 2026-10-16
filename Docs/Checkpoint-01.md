# Checkpoint 01 — native GPU notebook reader

Status: implementation and bounded validation; **not** a completed application or full performance certification. Updated 2026-09-12.

## Implemented behavior

- x64-only Visual Studio configurations and compile/build guards.
- Read-only nbformat 4 loading on a document worker, without Python execution. Original JSON retains unsupported representations and metadata. Failed loads leave the prior document intact.
- Classic Jupyter-style title, toolbar, code cells, prompts, saved text/error output, and selection. Unimplemented toolbar commands are disabled visually. The redundant native application menu is removed.
- Small-window layout with separated toolbar/status regions. Long source, raw/Markdown source, and text output can be reached with a measured horizontal scrollbar; vertical scrolling uses wheel and keyboard. Prose wrapping remains future work.
- Hardware Direct2D/DirectWrite drawing on D3D11; no CPU window bitmap upload or software device fallback. Text layouts are cached. The document model has no GPU ownership.
- HDR FP16/scRGB presentation and SDR fallback. Windows SDR white-level matching corrected the dark HDR UI. Monitor/display settings trigger capability refresh.
- Noto Sans regular/bold and Noto Sans Mono, privately loaded from bundled OFL font files. Text and vector shapes render at actual target DPI, including 200%. Modern swap chains do not stretch old buffers on resize.
- Event-driven render worker with vsync presentation; no recurring render or file-write timer in normal operation. Atomic document publication and coalesced view updates.

## Renderer decision

The planning documents proposed Vulkan. The first prototype uploaded a GDI-rendered window through Vulkan, which did not meet the user's hardware-rasterization requirement. It was replaced before the implementation checkpoint commit with Direct2D/DirectWrite on a hardware D3D11 device. This supplies native text shaping and GPU drawing while retaining a Windows 7-era API fallback and modern HDR presentation. The project no longer requires a Vulkan SDK or runtime.

## Evidence and limits

Host: Windows 10 Home 22H2, build 19045; hardware GPU (model redacted); HDR active with Windows SDR white scale 3.5 (280-nit reference UI white). Only this host is represented by the runtime results.

- Release and Debug application builds and notebook/color/raster unit tests passed during the checkpoint.
- Tests cover text/string-array input, UTF-8/UTF-16, execution counts, streams, plain MIME fallback, unsupported-output indication, error summaries, original-JSON preservation, 1,000 cells, malformed/missing files, invalid field types, nesting/size limits, color transfer, and DPI cache sizing.
- Interactive probes exercised 420/640/960/1200-DIP widths at layout scales 100%/150%/200%/100%, native resize-border hit testing, absence of a native application menu, vertical/horizontal navigation, minimize/restore, and both HDR and forced SDR presentation.
- GPU readbacks were inspected at physical output resolution, including 200% Noto text, narrow layouts, Unicode, and the `RIGHT_EDGE_SENTINEL` / `OUTPUT_RIGHT_EDGE` markers at the ends of long lines.
- Computer use launched the app and inspected native accessibility controls and the Open dialog. Screen capture failed on this host with `SetIsBorderRequired: No such interface supported (0x80004002)`. Later computer-use attempts were stopped by the user. GPU readbacks are renderer evidence, not a replacement claim that full desktop screenshot testing passed.
- The instrumented Release long-line run recorded max sampled UI work **0.78 ms**, max render preparation **3.64 ms**, and **zero additional presented frames and zero measured CPU time** in each of three consecutive two-second idle intervals. UI/render/loader thread samples were also zero. Final HDR and SDR runs recorded sampled UI maxima of 2.89 ms and 1.77 ms, with three quiet zero-CPU/zero-frame intervals each.
- One earlier long-line run recorded **78.125 ms process CPU over two idle seconds**, with no extra presentations. A repeat and the later instrumented run did not reproduce it. That original untraced sample remains unresolved. A later traced recurrence coincided with 330 mouse moves and 334 cursor/hit-test messages: it was input activity, not idle. The probe now retains input-active samples but requires three consecutive quiet intervals, each still subject to the same strict CPU/frame limits. See `validation/idle-with-pointer-input.json`. The anomaly is retained here and is not treated as disproven by a later pass.

The probe measures selected message-handler work, not every possible action. Preparation excludes GPU submission/presentation waits. There is no complete GPU-time, input-latency, or present-history trace yet, and no proof of zero dropped frames under sustained load. DPI scale probes simulate layout density on this host; they do not certify mixed-monitor movement on every Windows version. HDR readbacks are normalized to SDR for inspection and do not measure physical display luminance or color accuracy.

## Current boundaries and next work

Reader bounds: 32 MiB file, 10,000 cells, 1 Mi UTF-16 units per source/output preview, 128-level JSON nesting, and bounded total layout height. Oversized content is rejected rather than silently truncated. Full traceback formatting, rich Markdown, image outputs, editing, save/recovery, and kernels remain unimplemented. Runtime settings are not persisted yet; add known-folder AppData storage before adding state.

Next checkpoints should provide text selection/editing and document-safe persistence, broaden reader output support, and add real frame/GPU timing. Prose wrapping, per-cell horizontal scrolling, accessible custom controls, renderer device-loss tests, sustained long-document performance, and the idle anomaly remain tracked work. Windows 7/8/8.1/11 and physical HDR/SDR monitor transitions require additional hardware/OS evidence.

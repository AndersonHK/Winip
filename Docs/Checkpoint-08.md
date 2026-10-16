# Checkpoint 08 — large notebooks and visible file errors

Release **0.2.1**, 2026-09-14, following `a227e19`. Final development executable: `x64/Release/Winip-dev-18.exe`; installer: `Artifacts/packaging/Winip-0.2.1-x64.msi`. Dev-17 is the intermediate comparison build. Selected raw evidence and exact binary/source identities are in the validation summary (local archive: `Artifacts/archive/validation/checkpoint08-summary.json`; not distributed).

## Implemented behavior

The arbitrary 32 MiB read/save limit is removed. `NotebookInput.h/.cpp` owns a 64 KiB refill buffer and feeds the existing JSON parser through a single-pass iterator, without retaining a second full-file byte string. Cancellation is checked at refills; superseded requests are discarded. UTF-8 sequences, JSON escapes and strings may cross buffer boundaries. Parsing and document preparation remain on the loader worker.

`NotebookOutput.h/.cpp` streams through a 64 KiB staging-file writer. Unchanged cells and top-level metadata are serialized directly from immutable JSON; changed cells require only a cell-local serialization copy. Output is flushed completely before atomic publication. Semantic conflict checks compare documents without constructing two complete serialized clones. Existing Save As collision and external-change protection remain. File offsets and totals are not narrowed to a 32-bit write length.

Failed open, save or rename now displays an owned modal error dialog with operation, filename and reason, in addition to status text. The previous notebook remains available. The dialog is shown on the UI thread after releasing the shared lock; obsolete request failures are ignored. Missing-file, malformed-JSON and malformed-UTF-8 probes verify modal ownership, useful details and preservation of the previous document.

The reported notebook also exceeded the old per-cell source and saved-image limits. Read-time source/output preview caps are removed, while Windows text conversion bounds and checked layout arithmetic remain explicit. Saved PNG/JPEG decoding accepts source dimensions up to 16384 and encoded lengths up to the Windows decoder's DWORD range. Original pixels and MIME representations are retained. The artificial per-cell/per-notebook decoded-image caps are removed; available RAM now bounds those allocations. Unsupported decoding still produces a cell-local diagnostic without discarding data.

The GPU image source cache budget grows from 64 to 512 MiB, with the same 128-entry limit and LRU behavior. A single larger source may occupy an otherwise empty cache (up to the existing 16384-square decoder bound, roughly 1 GiB); effect intermediates and driver overhead are additional. Actual GPU capability/allocation failures remain visible diagnostics. This does not preallocate 512 MiB, change the text cache or reduce source resolution.

## Large-file evidence and limits

The actual `private-benchmark.ipynb` on the owner's specified share was read without executing any cells or writing back. It is 194,675,788 bytes (185.7 MiB), with 99 cells, 19 saved code-output PNG images and 406,516,928 decoded image bytes. Two source images are 6512×6727. Native load and layout succeeded; the final reader sample took 11.97 seconds. The largest Markdown source contains 14,063,481 characters.

Six large Markdown cells contain inline data-URI images. The existing Markdown renderer still shows their alt labels; this patch preserves those sources but does not implement Markdown images. The decoded/renderable PNG count above refers to code outputs, not those Markdown images.

A generated notebook containing distinct numbered 1 MiB metadata strings crossed the 4 GiB boundary with meaningful preserved data: **4,296,028,327 input bytes**, **4,296,040,647 saved bytes**, native load/layout/save/reopen and semantic equality passed. With the final buffered reader this took **28.95 seconds**, with approximately **8.04 GiB peak working set** while comparing two loaded copies. Its ordinary code cell was never executed. An earlier stream-adapter sample took 40.0 seconds; these are individual large-file samples, not a general throughput guarantee.

This is still a RAM-backed JSON DOM, not a paged or bounded-total-memory document. Four GiB is the owner's current acceptance target; terabyte-scale support is deferred. Remaining limits include 10,000 cells, 20,000,000 DIPs of document extent, 1 Mi-character source edits and Windows per-value conversion ranges. No claim is made that every possible 4 GiB arrangement is editable or renderable. Live kernel transport retains its earlier bounds; larger saved files do not expand the execution protocol.

## Performance review

The actual dev-16/dev-17 binaries completed two alternating-order resize matrices: six trials per binary for each of ordinary, long-line, image and 1000-cell fixtures. All 48 runs passed exact-size buffer/revision checks. Each issued 320 resize requests and presented 313 complete frames for the 313 changed sizes; duplicate dimensions account for the difference. The host ran at 144 Hz, with approximately 2.17 seconds per workload.

The first three trials showed image CPU +25% and ordinary-notebook CPU +13.5%; those samples remain in the evidence. Repetition narrowed the differences substantially and also exposed substantial baseline variation. Across all six trials, process CPU mean/median milliseconds were:

| Fixture | Dev-16 mean / median | Dev-17 mean / median |
| --- | ---: | ---: |
| Ordinary | 630.21 / 617.19 | 687.50 / 671.88 |
| Long lines | 721.35 / 679.69 | 591.15 / 507.81 |
| Images | 703.13 / 640.63 | 716.15 / 703.13 |
| 1000 cells | 682.29 / 671.88 | 690.10 / 632.81 |

The owner judged the repeated results within measurement error and requested a hot-loop audit. No frame drawing, resize synchronization, text-cache, cell-search, input or idle scheduling algorithm changed. Image lookup remains bounded by 128 entries; the higher byte budget reduces eviction opportunities. New file buffering, serialization and cancellation checks run on the worker. The error-dialog branch runs only after an operation fails; its test timer is opt-in. There are no added per-frame scans, allocations, readbacks or forced layout invalidations. Measured render-stage cycle counts were much closer than the noisier process CPU samples; this does not establish the cause of every fluctuation.

A same-process I/O algorithm comparison caught a real intermediate regression: the streambuf parser took 73.79 ms versus 62.23 ms on 16 MiB metadata. Replacing its per-byte stream access with an inlinable buffered iterator removed that overhead. In nine alternating trials, excluding trial zero as warmup, the final-reader median was **51.16 ms versus 60.05 ms** for the retained old algorithm. Ordinary load was 0.104 versus 0.103 ms, and 1000-cell load was 14.78 versus 15.06 ms. Save As medians were 2.46 versus 2.45 ms (ordinary), 5.99 versus 10.02 ms (1000 cells), and 45.13 versus 56.11 ms (metadata). Common current document preparation is shared by this benchmark; it isolates I/O algorithms rather than comparing old/new whole executables.

## Regression acceptance

Release and Debug model tests, including real Hello World/shared-state/error/interrupt/restart tests and Pillow, IPython JPEG and Matplotlib fixtures, pass. No Python installation or packages were changed. The former source-cap regression test now verifies successful loading above that cap. Added cases check UTF-8/JSON escapes crossing 64 KiB boundaries and cancellation after partial input.

All **160 GPU text-cache fidelity comparisons** and **72 image comparisons** pass. The added 4097×32 source checks the removed size boundary and far-edge pixels, with SDR/FP16 HDR, 100/125/200% DPI, natural/half-size drawing and cache cold/warm reuse. Tolerances remain unchanged. Maximum-size texture allocation, arbitrary profiles, device loss and low-VRAM hardware remain outside these bounded tests.

The initial application comparison passed functional checks and the ordinary UI deadline but recorded idle failures on both versions: dev-16 editing/navigator and dev-17 image/kernel each had a 15.625 ms process-accounting sample; navigator also had two presentations. A separate early editing sample had one presentation. They remain failed samples, not proof of idle compliance. CPU-accounting granularity alone does not establish the cause, and no threshold was relaxed. The final dev-18 interaction matrix passed editing (1.520 ms maximum ordinary UI), navigator (0.1511 ms), image (0.2026 ms), kernel (0.1413 ms) and window transitions (0.2892 ms ordinary UI); all candidate quiet intervals had zero extra frames and zero measured CPU. Dev-16 in that matrix had an unexpected trailing `T` in its code-edit check and another one-tick image-idle failure; these are retained, with no established attribution.

The four dev-18 resize fixtures also passed. A visual long-line sample reached all four sizes and the draggable end marker but ended before restore after recording active input; a visual image sample had two idle presentations. Those are retained failures. The subsequent no-readback long-line and image runs passed all existing gates, including three quiet intervals. Fresh app-owned HDR/200% image and long-line captures were inspected; screenshot readback is separate from accepted timing. The last source hardening only affects failed-file diagnostics and forbids copying buffered stream owners; the final malformed-UTF-8 modal, long-line/image probes and MSI verification use that final binary.

The 0.2.1 MSI passes read-only properties/UI routing, extracted payload hashes/icons/AppPaths and native ICE validation with warnings as errors. Shared component identities remain compatible with 0.2.0. Exact results and release hashes are recorded in the linked validation summary.

No live installation, repair, upgrade, removal or file-association mutation is performed by this checkpoint. Those checks remain with the owner/disposable VM. Host evidence does not certify Windows 7–11, sustained 160 Hz, physical HDR, zero GPU idle time or outside-window compositor behavior.

# Checkpoint validation

Use the smallest suite that covers a change's real failure modes. A documentation-only checkpoint needs link/status/reference verification; rendering, document, or kernel changes need the relevant executable tests. Record failures and measurement limitations even when a later run passes.

## Commands and evidence

From the repository root:

```powershell
python Scripts/build.py Release --tests
python Scripts/build.py Debug --tests
python Scripts/build.py Release --name Winip-dev-next
python Scripts/probe.py Release --name Winip-dev-next --sdr
python Scripts/probe.py Release --name Winip-dev-next --long-lines
python Scripts/interaction_probe.py Release --name Winip-dev-next
python Scripts/interaction_probe.py Release --name Winip-dev-next --no-snapshots
python Scripts/editing_probe.py Winip-dev-next
python Scripts/editing_probe.py Winip-dev-next --no-snapshots
python Scripts/resize_probe.py Winip-dev-next
```

The name is an example; choose an unused build name if another executable is open. Do not close a user's working instance simply to replace a locked binary. Keep the generated fonts directory beside the executable.

The model test executable also runs 160 offscreen hardware-GPU cache comparisons: SDR/FP16 HDR, 100/125/150/200% DPI and returning to 100%, fractional origins, Unicode, multiple lines, changed source/color, ellipsis, clipping, oversized-line fallback, cache reuse/eviction/clearing. It writes `Artifacts/text-cache-fidelity.json`; the accepted tolerance is one normalized 8-bit channel level. This is a cache-versus-direct-render check, not physical monitor color calibration.

Notebook model checks cover empty/unmeasured/measured states, shared immutable snapshots, retained original JSON, rejected layout replacement without partial mutation, and coherent downstream cell positions after relayout. TitleEditor checks cover selection replacement, bounded caret positions, surrogate-pair navigation/deletion, inactive edits, input limits, and preserving an existing long filename when editing begins.

Window probes require an interactive unlocked desktop. The user handles manual computer-use checks at checkpoints. Run scripted validation by default; external mouse/keyboard automation requires a new user request. App-owned probes identify themselves and close themselves; incidental mouse/keyboard activity can invalidate a controlled reference. Keep those samples and rerun the affected phase rather than lowering the threshold. The initial idle gate requires three quiet two-second intervals, no extra presentations, and at most 10 ms measured process CPU per interval.

Image readback is intrusive. Separate visual probes from CPU/frame timing runs with `--no-snapshots`. `Present` wall time includes vsync waits; CPU cycles, CPU time, GPU time, and elapsed wall time are different measurements. The continuous resize probe makes 320 requests, including seven repeated dimensions; 313 presentations can correctly cover every changed size. Do not label sparse idle presentations as dropped frames.

## Evidence retention and cleanup

Keep raw logs, JSON results, screenshots and generated notebooks in `Artifacts/`.
[Publication.md](Publication.md#what-belongs-in-git) defines the source-only Git
boundary and ignored build/distribution locations. Public documentation may contain
concise reviewed summaries, commands and limitations, following
[Writing-Guide.md](Writing-Guide.md).

After validating and reviewing a run, remove disposable large notebooks, roundtrip
copies, BMP captures, obsolete executables and intermediate build trees. Compare
SHA-256 and byte size to detect identical generated files; retain one useful copy
and record aliases in the cleanup manifest. Do not accumulate copies merely
because they came from different runs. Use `--no-snapshots` for timing checks.

Retain lightweight local logs, generation parameters, build/source identity and
pass/fail results when useful. Before deletion, write an ignored manifest with
repository-relative paths, byte sizes, hashes and the reason for removal. Keep
only selected visual evidence needed for an unresolved failure or documented
regression, preferably lossless PNG. Investigate failed runs before discarding
their essential diagnostics. Never remove user notebooks, maintained fixtures,
required package inputs or active comparison baselines.

The pre-publication raw reports formerly under `Docs/validation/` now live only
in `Artifacts/archive/validation/`. Historical summary references to those files
are local provenance labels, not downloadable evidence. Sanitized versions omit
profile/interpreter paths, private notebook names and device identities; their
old embedded hashes identify the original measured files, not edited public docs.

## Fixture catalogue

Packaging checks are separate from window probes: `python Scripts/packaging_tests.py` checks the built MSI, extracted payload, executable icon resources and native Roaming path service. Native MSI ICE validation and a disposable-VM lifecycle script are documented with observed results and remaining acceptance in [Packaging.md](Packaging.md).

| Existing fixture | Purpose | Does not prove |
| --- | --- | --- |
| [visualization.ipynb](../Tests/Fixtures/visualization.ipynb) | Eight code/Markdown/raw cells, Unicode, saved streams/errors/plain alternatives and unsupported rich output | Real Plotly/Bokeh execution or image rendering; several outputs are deliberate textual placeholders |
| [long-lines.ipynb](../Tests/Fixtures/long-lines.ipynb) | Twelve code cells, horizontal end markers, large text extents | Wrapped editing, image/pan interaction, or arbitrary huge-cell latency |
| [large.ipynb](../Tests/Fixtures/large.ipynb) | 1,000 code cells; parsing, geometry, visible-cell behavior | 1,000 live executing kernels or rich output stress |
| [malformed.ipynb](../Tests/Fixtures/malformed.ipynb) | Rejection without losing a previously valid document | Every malformed/adversarial representation |

Tests also generate disposable files for invalid structures, rename collisions, and Python selection. Do not overwrite reference notebooks during interactive editing tests; work on copies and compare preserved data on reopen.

## Current editing acceptance and upcoming coverage

- Editing now has model and app-owned checks for Unicode source, click-to-edit, Markdown deselection, delete/undo, mode shortcuts, insert, save races, and preserved unknown JSON on reopen. Model tests cover rich style ranges, entities, last-cell deletion, conversion and serialization failures. The probe uses a disposable notebook and does not execute its code. Clipboard/IME/full grapheme behavior, close-dialog interaction, arbitrary huge-cell deadlines, and accessibility still need coverage.
- Kernel: chosen interpreter identity, request-to-cell identity after insert/reorder, execution ranges/order, error/interrupt policy, output floods, stdin, crash/disconnect, restart generations, and shutdown while busy. Editing and window operations must remain responsive.
- Images/plots/tables: real generated outputs, multiple MIME alternatives, replacement/clear-display updates, large output navigation, alpha/color/DPI fidelity, bounded decoding/cache storage, and preservation of unsupported representations.
- Dashboards/widgets: explicit supported interaction semantics and lifecycle; a screenshot of a dashboard does not establish interactive compatibility.

The current host evidence does not certify Windows 7–11, sustained 160 Hz deadlines, zero GPU time at idle, device loss, every monitor transition, or physical HDR luminance. Keep these as explicit targets in the [rendering contract](Rendering-Contract.md).

Kernel checkpoint: `python Scripts/kernel_probe.py Winip-dev-11 --no-snapshots` uses a disposable Hello World notebook, runs a real selected kernel, checks shared state/edit-during-execution/save/reopen and three quiet intervals with separate app and kernel-job CPU accounting. Set `WINIP_TEST_KERNEL=1` in the build command's temporary environment to include real-kernel model tests; normal model runs remain independent of installed kernel packages. `Scripts/probe.py --long-lines` also exercises cell scrollbar drag to the middle and both ends through the window handlers.

Navigator checkpoint: `python Scripts/navigator_probe.py Winip-dev-14` uses the 72-cell unequal-height `navigation.ipynb` fixture and app-owned down/move/up events. It checks end-to-end thumb mapping, heading extraction, bounded animation, stationary-hold idle, 200% DPI narrow reveal/hide, and three quiet intervals. Add `--no-snapshots` for timing without GPU readback. Model checks exercise the actual paper/shadow visibility boundary (1284 versus 1286 DIPs), including the former erroneous 1200-DIP always-visible case. No notebook code runs in the navigator fixture.

## Kernel/images/settings and release 0.2.0

`python Scripts/image_probe.py Winip-dev-16` generates a disposable Pillow image notebook and checks default SDR, busy/idle indicators, real output, HDR toggling, 200% DPI, MIME save/reopen and a fresh counter after reopening. Use `--no-snapshots` for timing without readback. The probe uses its own settings directory. An app-owned probe does not authorize executing unrelated user notebook cells.

Release/Debug model tests now add 48 GPU image comparisons (tagged/untagged, SDR/HDR, 100/125/200% DPI, natural/half display size, cache cold/warm). Analytic color/alpha samples use a 2/255 normalized tolerance; this is not a complete ICC/physical-display certification. Tests also reject unsupported capability and oversized derived display dimensions, retain original malformed MIME data, and verify settings persistence/recovery. `images.ipynb` and `images-icc.ipynb` contain full-source 512×256 RGBA output; the latter embeds an sRGB ICC profile.

`WINIP_TEST_KERNEL=1` adds installed-environment kernel tests, including Pillow, IPython JPEG and Matplotlib inline PNG. These tests require those packages in the selected Python and do not install them. The optional `WINIP_TEST_LOCAL_MODULE` path is an explicitly authorized host-specific import-only check; inspect module top-level effects before using it with any other module. The owner's notebook data/save cells are excluded.

`python Scripts/packaging_tests.py --ice --previous-msi Out/Packaging/Winip-0.1.0-x64.msi` validates the current MSI plus old/new component identity without installing. See [Checkpoint 07](Checkpoint-07.md) for accepted and failed samples, limits and release hashes.

## Large-notebook patch 0.2.1

```powershell
python Scripts/large_file_probe.py --mib 4097
Out/Tests/x64/Release/WinipTests.exe --load-notebook .\example.ipynb
Out/Tests/x64/Release/WinipTests.exe --io-benchmark .
python Scripts/open_error_probe.py Winip-dev-18
python Scripts/performance_compare.py Winip-dev-16 Winip-dev-18 --group resize
python Scripts/performance_compare.py Winip-dev-16 Winip-dev-18 --group interaction
python Scripts/packaging_tests.py --ice --previous-msi Out/Packaging/Winip-0.2.0-x64.msi
```

`--load-notebook` decodes and measures only; it executes no cells and writes nothing to the source. `--roundtrip-notebook` creates a sibling `.roundtrip.ipynb` and is reserved for disposable fixtures. The large-file script creates distinct numbered 1 MiB metadata values, exercises the native roundtrip and records peak working set; allow roughly 9 GiB disk and 9 GiB available RAM for the 4097 MiB case. The parser retains a JSON DOM despite its 64 KiB input buffer; this is not a bounded-total-memory benchmark.

The I/O benchmark alternates nine trials of retained 0.2.0 algorithms and the new algorithms in one test binary, including ordinary, 1000-cell and 16 MiB metadata fixtures. Common current document preparation is shared: this isolates algorithm changes, not complete old/new executable behavior. Preserve all trials and identify warmup separately. The window comparison instead launches actual baseline/candidate executables sequentially; keep builds and other heavy checks outside timing runs.

Release/Debug suites now include refill-boundary UTF-8/JSON escapes and cancellation after partial input. GPU image comparisons increase from 48 to 72 with `images-wide.ipynb`: a 4097×32 source, including pixels near the far edge, SDR/HDR, 100/125/200% DPI, natural/half size and cold/warm cache paths. This extends source-size boundary coverage; it is not a complete maximum-texture or low-VRAM certification. [Checkpoint 08](Checkpoint-08.md) records current results and failed samples.

## Responsive images and loading: 0.2.2

`python Scripts/media_probe.py Winip-dev-24` exercises fit/full/fit, horizontal image access, 200% DPI, eager image readiness, first/warm image sweeps, reloading during native resize and quiet idle. An optional second argument accepts the workspace benchmark copy; it never executes notebook cells. Omit screenshots with `--no-snapshots` for timing. The fixture `markdown-images.ipynb` includes wrapping/Unicode, a large data PNG, attachment PNG, relative JPEG, missing/remote diagnostics and wide code output. Its neighboring `relative image.jpg` is required.

`Out/Tests/x64/Release/WinipTests.exe --load-benchmark <absolute-notebook-path>` reports complete CPU parse/decode/initial text preparation. Stage totals are nested, not additive. `--gpu-load-benchmark` additionally fits a 1600x900 DIP viewport and creates hardware textures; its total includes creation of a separate benchmark GPU device. Compare the same mode/functionality, retain image counts/decoded bytes, and distinguish these from application first presentation. See [Checkpoint 09](Checkpoint-09.md) for the private benchmark hash, timing modes, cache-pressure failure and corrected results.

The GPU suite now runs 144 image comparisons, covering both original bitmap uploads and loader-prepared D3D textures with the same source/color/alpha/DPI/HDR tolerances. The 160 text-cache comparisons remain unchanged. Native-window probes and real-kernel connection-file permissions require execution outside the host sandbox; test environment flags affect only the child process.

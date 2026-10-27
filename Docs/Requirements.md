# Product requirements

## Core requirements

The application is designed around these requirements:

- x64 only; target Windows 7–11 where platform/driver capability permits. Older-OS support needs runtime evidence.
- A hardware GPU is required. Prefer GPU work and spare the CPU; no full-window CPU rasterization or software graphics fallback.
- HDR is first class but optional and OFF by default, following the Windows 10 capture investigation. Persist the user choice in Roaming Winip settings; retain deliberate SDR behavior and appropriate HDR UI luminance.
- Every UI element rasterizes at full destination resolution. Downscaling is allowed; upscaling only when the original source lacks sufficient pixels. See the [rendering contract](Rendering-Contract.md).
- Classic Jupyter visual organization and a long-term goal of **Cantor feature parity**, not merely inspiration. See the [parity tracker](Parity.md).
- A resizable, small-window-safe UI without a redundant native application menu. Long source/output lines must remain fully accessible through scrolling or wrapping.
- Open-source scalable fonts, rendered crisply at 200% and other DPI scales.
- Meet the active refresh deadline: 6.25 ms at 160 Hz (the earlier 6 ms rule was a rounded guide). Ordinary updates should not miss a frame; a substantial update arriving mid-frame may miss at most one.
- User-driven resize, maximize, minimize, and fullscreen changes are synchronous and atomic: no partial frames, stretched prior frame, or old-size presentation. Expensive document/kernel work remains asynchronous. Near-zero idle CPU/GPU work.
- Larger restored windows and maximized launch; Alt+Enter fullscreen; inline title renaming; a custom dropdown menu row above the separate toolbar row.
- Python selection honors explicit and activated-environment overrides, inherited terminal PATH, and otherwise registered user PATH before system PATH, followed by registered installations. Automatic desktop discovery may prefer an already kernel-compatible registration; explicit/activated/terminal selections remain fixed.
- Validate incremental checkpoints with scripted notebook fixtures; the user handles manual computer-use checks, and commit stable progress. Record limitations instead of claiming untested OS, hardware, or performance guarantees.

## Intended experience

Double-click an `.ipynb` file in Explorer and open a dedicated notebook document application. Immediately display saved code cells, explanatory text, and supported outputs. Offer obvious Run, Run All, Interrupt, and Restart controls. Editing and scrolling should remain responsive while Python is busy.

Winip should be useful as a notebook reader even when the required Python interpreter or packages are unavailable. Opening a file should not automatically execute its code.

## Confirmed preferences

| Area | Requirement or preference |
| --- | --- |
| Platform | Windows x64 is the intended target; Windows behavior and reliability take priority. |
| Application type | A dedicated `.ipynb` application with a UI designed around cells and outputs. |
| Frontend | No JavaScript frontend and no browser engine, including Electron or WebView-based rendering. |
| Native implementation | Native C++. The preliminary Vulkan proposal was replaced with hardware Direct2D/DirectWrite on D3D11 in the first checkpoint; see [renderer decision](Checkpoint-01.md#renderer-decision). |
| Python ownership | Use one existing default Python installation for ordinary work, with explicit repository-specific exceptions. |
| Packages | Preserve the ability to install and update Python packages with that interpreter's pip. |
| Storage | Application-generated state belongs in AppData rather than directly under the user-profile root. |
| File opening | Support Explorer opening and an `.ipynb` file association through a normal Windows installation/setup flow. |
| Packaging | Orange notebook/window/Python icon without the program name, embedded through 256px; MSI to Program Files with folder selection; a checked-by-default association page; native Windows uninstall and registry/installed-file cleanup. |
| Writable application state | Use the current user's **AppData\Roaming\Winip** folder, including redirected Roaming profiles; not the AppData parent. Notebook documents retain their chosen locations. |
| Reliability | A stable everyday application; depending on another application's unstable Windows nightlies is unattractive. |

The performance requirement is an engineering target, not a claim that every cross-platform application must perform poorly. Architecture, platform integration, maintenance, and measured behavior matter independently.

## Feature direction

The feature scope includes code/Markdown editing, cell insertion, Python execution, selected/all/above/below execution, restart/shutdown and cooperative interruption. Editing, selected/all execution and the main kernel controls are implemented; additional execution ranges and richer output remain planned. Native tables, interactive graphs and embedded dashboards are longer-term work. See [Parity.md](Parity.md) for current capability and [Roadmap.md](Roadmap.md) for sequencing.

## Initial milestone scope

- Open, create, edit, and save notebooks while preserving unsupported content and metadata.
- Code and Markdown cells, cell insertion/deletion/reordering, selection, and undo/redo.
- Saved text output, errors, and static images, with an explicit supported-format list.
- Select an existing Python executable and show its identity and kernel state.
- Execute cells, interrupt work, restart the kernel, and handle kernel failure.
- Recover unsaved edits after an application failure.
- Native open/save dialogs, clipboard integration, keyboard navigation, and correct high-DPI behavior.

This list records the initial scope. The parity tracker distinguishes completed items from remaining work; it is not a claim that every item is implemented.

## Compatibility boundaries

Reading `.ipynb` does not imply implementing every Jupyter extension or output renderer. Preserve unknown metadata, attachments, and output representations when saving even when the UI cannot display them. Semantic round-trip preservation is the initial target; byte-for-byte JSON formatting preservation is unresolved.

JavaScript-based visualizations and ipywidgets cannot simply run in a frontend that excludes JavaScript. Provide supported static representations when available and a clear indication otherwise. Native replacements are possible future work; silent deletion of unsupported content is unacceptable.

General-purpose IDE functionality, a bundled Python distribution, automatic environment creation, remote collaboration, other operating systems, and comprehensive widget compatibility are outside the initial proposal. Additional language kernels may eventually be possible through the Jupyter protocol, but Cantor's broad language support is a long-term parity requirement, sequenced after the initial Python/document work.

## Later opportunity: parallel execution

Expose parallel work through explicit inputs, outputs, worker counts, progress, and cancellation. Do not describe this as automatically making arbitrary sequential Python code multicore. See [architecture](Architecture.md#parallel-execution) for the proposed semantics.

## Performance tradeoffs

Performance optimizations must preserve all functionality and presentation, including live updates during resizing. Bounded memory/GPU caches are encouraged when they measurably reduce CPU work; they must not freeze updates, reuse insufficient-resolution pixels, or bypass invalidation. Rapid repeated edge dragging is an explicit performance-validation scenario.

## Implementation conventions

Source organization and lifetime rules are maintained in [Coding-Guide.md](Coding-Guide.md).
Presentation rules are maintained in [Rendering-Contract.md](Rendering-Contract.md),
and controls/shortcuts in [UI-Style-Guide.md](UI-Style-Guide.md). Evidence and cleanup
follow [Validation.md](Validation.md). These guides define the implementation
constraints; this document defines product scope.

## Settings and release policy (2026-09-13)

Persist HDR and file-open folder policy under Roaming Winip. Default to the last opened notebook folder, initialized from the Downloads known folder; allow a fixed folder instead. Kernel state uses gray/green/red circles and busy Run all uses a refresh symbol. Opening a notebook starts a fresh session independently of saved execution counts. Before GitHub publication use `0.major.minor`, with substantial changes in major and fixes in minor; see [release identity](Releases.md).

## Large-notebook patch scope (2026-09-14)

The maintainer requests at least 4 GiB file support for 0.2.1; larger/terabyte-scale handling can wait for a future patch. File-open failures must be conspicuous error windows, not status-only messages. Preserve ordinary UI, resize, idle and image fidelity behavior, with baseline comparisons before the checkpoint. A size acceptance test must contain real preserved JSON payload, not only whitespace or a sparse file.

## Kernel setup requirement (0.3.0)

Expose missing kernel support before execution and explain startup failures in
full. Keep essential kernel status visible by dropping optional footer columns
first. Automatically check requirements and offer default-on, skippable guided installation
in the MSI and application, with explicit destination confirmation;
a working kernel must not trigger pip recommendations. Select stable compatible
versions without unnecessary upgrades, detect completed external installations,
and offer the official route after failure. See [Python and storage](Python-and-Storage.md)
for implemented scope and [the acceptance plan](Archive/Kernel-Setup-Plan-0.3.md).

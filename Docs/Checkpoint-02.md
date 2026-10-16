# Checkpoint 02 — window composition, menus, renaming, Python discovery

Status: stable behavior checkpoint; the overall Cantor/parity and performance goal remains active. Validated on the Windows 10 / hardware GPU (model redacted) host recorded in checkpoint 01. The active display mode reports 144 Hz (6.944 ms/frame); the user's 160 Hz target means 6.25 ms/frame.

## Behavior

- Normal launches maximize. The restored size targets 1600×1000 DIPs, bounded by 90% of monitor work area. Alt+Enter toggles borderless fullscreen, ignores key repeat, and preserves prior maximized/restored placement.
- A separate custom menu row appears above the toolbar with File, Edit, View, Insert, Cell, Kernel, Widgets, and Help. Dropdowns support mouse selection and keyboard navigation. Current commands work; future editing/kernel/widget actions remain disabled. The title rename hint sits beside the measured title.
- Click the title or use F2 to rename the current notebook file. Enter applies, Escape cancels. The GPU-rendered single-line editor supports selection, arrows, Home/End, Ctrl+A, Delete/Backspace, and bounded Unicode input. Rename is queued to the document worker, stays in the same folder, rejects invalid/reserved Windows names, and never replaces an existing file. Notebook bytes are unchanged.
- The native bottom scrollbar is removed. Overflowing cells expose thin clickable tracks; horizontal keys/wheel and Shift+wheel remain available. Short cells no longer disappear when panning a longer cell. Thumb dragging and per-cell independent scroll state remain future work.
- Python is discovered and identified asynchronously. Priority: explicit `--python`, activated venv, activated Conda, inherited PATH for recognized terminal parents, otherwise registered user PATH before system PATH. Explicit/activated failures are reported without silently switching environments. A hidden, bounded, isolated stdlib subprocess reports interpreter path/version/bitness; no notebook code, site hooks, installation, or package changes run. Current host discovery identifies Python 3.9.7, 64-bit. Zero-byte Store execution aliases and shell functions/aliases need an explicit interpreter path. Kernel execution is the next functional stage after the structural refactor.

## Resize correction

The user reproduced black rectangles outside the window after shrink–expand–shrink, with the affected regions depending on earlier sizes. App-owned GPU readbacks did not expose that compositor artifact. Keeping larger HWND buffers did not resolve it.

Windows 8+ now uses DirectComposition with no HWND redirection bitmap. A complete exact-size frame is rendered and presented during `WM_NCCALCSIZE`, before the native size transaction completes. The composition visual uses an identity transform and physical-pixel bounds. DirectComposition is dynamically loaded from the system directory; the Windows 7 hardware path remains available but unverified. The user tested `Winip-dev-05.exe` and explicitly reported that the black bars were gone, then described the application as stable.

The approach was informed by a [published reproduction](https://github.com/bigfatbrowncat/noflicker_directx_window) and the [Microsoft composition swap-chain API](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition). No source was copied from the reproduction.

## Validation and limits

- Release/Debug builds and model, filename/rename, title-edit, menu-layout, interpreter-order, color, and physical-resolution tests.
- Integration probes: default maximization; fullscreen client/monitor coverage; placement restoration; title click/F2/edit/cancel/apply; invalid-name rejection; menu keyboard rename; absence of the native bottom scrollbar; repeated shrink/expand cycles; exact-size composition allocations and populated frame edges.
- A disposable notebook is renamed and its byte hash checked. Existing-destination collision tests verify both files remain intact. Reader fixtures cover Unicode, long lines, malformed files, and 1,000 cells.
- Computer use launched the app and verified Alt+Enter removed native title-bar controls. Desktop capture repeatedly failed with `SetIsBorderRequired: No such interface supported (0x80004002)`. The outside-window glitch acceptance therefore comes from the user's live test, not an invented screenshot verification. App GPU readbacks verify content/geometry only.
- Screenshot readback is intrusive and excluded from performance runs. Raw DXGI statistics and transition/command timing are recorded separately. Sparse idle presentations must not be classified as missed frames.
- The final no-readback run recorded max sampled ordinary UI work 0.544 ms, render preparation 5.139 ms, and native window-transition wall time 19.530 ms. This does **not** certify the 160 Hz / at-most-one-missed-frame requirement. Complete GPU/present deadline analysis, sustained 160 Hz tests, and broader OS/monitor coverage remain open.
- Accessibility automation disturbed idle measurements through repeated `WM_GETOBJECT` requests; the disturbed sample is retained. A user also manually resized/closed a probe window while its idle phase was running; those input-active samples are not idle acceptance evidence.
- Final SDR and long-line probes passed, including restoration and three consecutive two-second quiet intervals with zero measured process CPU time and zero additional presentations.

See `validation/checkpoint-02-*.json` for retained measurements. Current functionality is still a cell viewer plus filename editing; cell editing/saving, full Markdown/images, and a connected Python kernel remain unimplemented.

## Next checkpoint — OOP refactor

No new feature scope is needed for this checkpoint. Replace the global application state and free-function orchestration with coherent classes. Use one primary class per matching header/source pair, explicit public/private interfaces, and constructor/destructor lifetime guarantees.

Proposed boundaries: `Application`, `NotebookWindow`, `Notebook`, `NotebookView`, `NotebookLoader`, `TitleEditor`, `MenuBar`, `PythonDiscovery`, and the existing `Renderer`/`FontSet` owners. Headers declare their interfaces; `.cpp` files implement them. Resource owners are noncopyable; immutable document/view snapshots cross worker boundaries; worker shutdown and native handles are owned by destructors. Keep document path/title/data and load/rename state consistent behind class methods rather than independent public fields.

Acceptance: unchanged GPU/HDR behavior, resize composition timing, notebook/rename fidelity, interpreter selection, menus, DPI/long-line access, and idle behavior. Re-run the existing checkpoint suites and commit the structural refactor separately.

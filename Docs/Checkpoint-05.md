# Checkpoint 05 — real Python kernel and notebook controls

Candidate `Winip-dev-11.exe`; build hash and raw evidence (local archive: `Artifacts/archive/validation/checkpoint-05/manifest.json`; not distributed). Based on `5954dd9`, including the user's packaging/icon work. No Python installation or package changes. Manual desktop and installer testing were left to the user.

The selected Python runs a real persistent ipykernel through a bundled bridge. Connect explicitly or run a cell to connect. Selected-cell execution and Run all produce saved stdout/stderr/error/result outputs; Run all stops at the first Python error. Another cell can use the existing namespace afterward. Code never runs merely by opening a notebook. Kernel starts, execution and output preparation run on a worker; completed outputs publish atomically against stable cell identity and matching source, preserving unrelated edits. The eight-bundle UI queue and 1 MiB/4096-message cell-output limit bound frontend delivery; exceeding the bridge limit reports an error and stops the session rather than silently truncating it.

Toolbar: separate + Code and + Markdown buttons, Run, Run all where space permits, and a working selected-cell type dropdown. Cell-menu type changes are enabled. Ctrl+Shift+Enter runs all, Ctrl+Shift+K connects, Ctrl+Shift+. force-stops. Ctrl/Shift+Enter run code or render Markdown. Both #test and # test render headings. Cell overflow scrollbars are now draggable, including both ends, with capture-release handling.

`KernelController`, `NotebookToolbar` and `CellScrollbar` have matching header/source pairs. The runtime bridge is an explicit ad-hoc/MSI payload; all app-owned mutable runtime files are under user state, and notebooks remain at their chosen locations. [Packaging review](Packaging.md#kernel-integration-review-checkpoint-05) records compiler cleanup and the distinction between an unelevated app and the current per-machine MSI.

## Validation

- Release/Debug application and model builds passed; each model suite passed 160 SDR/HDR/DPI GPU-cache comparisons.
- Real kernel tests passed Hello World, persistent variables, exact interpreter identity, Unicode/stdout/stderr/result output, error stopping Run all without losing the kernel, late output rejection after edit/delete, forced stop while busy and fresh-session reconnect.
- App-owned Hello World probe passed connection, Run all, editing/resizing during execution, save/reopen, and three two-second idle intervals. The final run recorded no new frames and 0 ms measured CPU per interval for both the app and its bridge/kernel job.
- Existing editing/save/Markdown, title/menu/fullscreen, and long-line/DPI/idle probes passed. The long-line probe exercises actual cell-scrollbar down/move/up handlers and checks middle/end positions and release.
- Intermediate failures were retained: a visual idle sample contained mouse/wheel input; a stop/reconnect test exposed reuse of a terminated Windows job. Quiet measurements and a fresh job per kernel session resolved those cases. No performance gate was lowered.

Host Python 3.9.7 with ipykernel 5.5.3 was used, on the recorded hardware GPU (model redacted) / 144 Hz host. These bounded tests do not certify Windows 7–11, sustained 160 Hz deadlines, physical HDR calibration or arbitrary huge cells. Python dependencies must already exist in the selected interpreter; missing modules surface as connection errors.

Current limits: complete output is published at execution completion, not streamed live; stdin is disabled, rich MIME data is preserved with current text fallbacks, and updates emitted after execution idle are not handled. The stop command is explicitly forceful; graceful interruption, restart/shutdown, crash monitoring while idle, session-directory cleanup after forced termination, richer output/backpressure and further run ranges remain subsequent work. No installer was built or installed for this slice.

The next requested slice adds a floating vertical notebook navigator with animated Markdown headers and cell-density marks while dragging.

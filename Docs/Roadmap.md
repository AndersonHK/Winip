# Incremental roadmap

Unresolved investigation: [native HDR screenshot/snipping fidelity](HDR-Capture-Investigation.md). Vulkan presentation, 80-nit UI dimming and a replacement Print Screen handler did not meet acceptance. The experiment worktrees were removed; no candidate fix is shipped.

The owner requested small, validated steps. This is the current sequence; it supersedes the [initial roadmap](Archive/Roadmap-initial.md). No dates or universal compatibility promises are implied. The full [Cantor parity target](Parity.md) remains active.

Foundation completed in bounded host tests: GPU reader; responsive custom controls; title rename; interpreter discovery; synchronous resize composition; full-resolution GPU text reuse. See [the checkpoint index](README.md#current-state).

## Implemented: large-notebook patch 0.2.1

[Checkpoint 08](Checkpoint-08.md) removes the 32 MiB open/save limit, streams file input/output, and presents owned operation-error dialogs while preserving the current notebook. A meaningful >4 GiB metadata roundtrip and the owner's 185.7 MiB notebook load are verified without running its cells. Four GiB is the current acceptance target; terabyte-scale paging, unrestricted cell counts/layout extents and large-source editing remain future work. Existing renderer/input loops retain their work bounds.

## Implemented: first source-editing slice

Packaging is also implemented as a separate checkpoint: orange executable icons, an x64 MSI with directory selection and optional `.ipynb` registration, a Roaming known-folder service, and native MSI uninstall. Build/payload/ICE checks are recorded in [Packaging.md](Packaging.md); elevated install/repair/uninstall and wizard acceptance on a disposable VM remain unverified.

The first [structural slice](Checkpoint-03.md) encapsulates Notebook snapshots and TitleEditor state, and pairs FontSet with its matching files. Application/window/view, menus, and worker lifecycle ownership remain to be extracted. Keep these structural slices reviewable separately from feature changes. The [editing checkpoint](Checkpoint-04.md) now provides the following initial subset; advanced input and ownership extraction remain future work:

- Click a code cell to edit its source, with caret/selection and complete long-line access.
- Click a Markdown cell to edit its source; preserve its kind and attachments. Deselecting Markdown renders a native rich preview; complete Markdown/math remains upcoming.
- Add code or Markdown cells above/below the selection, including in an empty notebook. Give cells stable identity and preserve selection through insertion.
- Add undo/redo, dirty-state protection, saving/reopening, and a safe response to close/open while edits are unsaved. Clipboard/IME/accessibility work must be explicit, with limitations recorded rather than hidden.

Acceptance: edit the intended cell, navigate/insert without corrupting adjacent cells, preserve Unicode and unknown metadata/output fields, save/reopen without data loss, and retain full-resolution updates while resizing. Use disposable .ipynb copies. Avoid a whole-notebook copy/reparse/reflow on every input. Revisit [Pretext](References.md#pretext--prepared-text-and-arithmetic-relayout) before introducing width-dependent wrapping.

## Implemented: first kernel and Hello World checkpoint

[Checkpoint 05](Checkpoint-05.md) demonstrates `print("Hello, world!")`, persistent state, selected/all execution and output preservation. The requested animated vertical notebook navigator is implemented in [Checkpoint 06](Checkpoint-06.md). [Checkpoint 07](Checkpoint-07.md) implements real PNG/JPEG and static Matplotlib output, interpreter selection, cooperative interrupt/restart, settings, fresh per-document sessions and versioned MSI maintenance. Next: native tables, graph breadth, local import diagnostics and explicit parallel work, with the concrete [next-slice plan](Next-Slices.md); further execution ranges remain open.

Launch/connect through the selected existing 64-bit Python. Verify required packages without silently installing them or choosing another interpreter. Show clear starting/idle/busy/disconnected/error state. Display the notebook immediately even when Python is unavailable.

Start with one code cell: correlate stdout/stderr, errors, results, completion and busy/idle transitions to the originating request/cell. Add deterministic execution ranges once that lifecycle works. Demonstrate shared state with one cell defining a variable and another using it; verify sys.executable matches the selected interpreter.

### Kernel actions and execution ranges

| Requested action | Proposed precise behavior / acceptance |
| --- | --- |
| Run cell | Execute the selected code-cell source snapshot once; Markdown is not sent as Python |
| Run all | Queue code cells in document order; execute each once and expose progress |
| Run above | Code cells strictly before the selected cell, in order |
| Run below | Code cells strictly after the selected cell; offer a separately named “from selected cell” command if useful |
| Interrupt execution | Request interruption while retaining a usable kernel and its surviving namespace; verify another cell can run afterward |
| Stop/shut down kernel | End the session, clear the queue, and enter disconnected state without losing document edits or saved outputs |
| Restart kernel | Start a fresh session with an empty namespace; reject late results from the old generation |
| Restart and run all | Explicit combined command; ordinary restart does not unexpectedly execute the notebook |

Range inclusion rules are proposed defaults, not an additional user instruction. Specify queue behavior on error/interrupt and edits during a queued batch before shipping batch execution. Capture stable cell IDs and source revisions; never rely solely on a mutable index. Interruption may depend on cooperative runtime/extension behavior; report failure/timeout instead of disguising a forced shutdown as successful interruption.

Acceptance includes a long-running cell, exceptions, output floods, input requests, missing packages, crash/disconnect, shutdown while busy, and insert/reorder during execution. Editing, scrolling, and native geometry operations must stay responsive. Worker publication is atomic and bounded. Running code on file open is never the default.

## Subsequent rich-output slices

The user prioritizes image, graph, and table libraries; embedded dashboards are explicitly the least important. Pillow/IPython PNG/JPEG and Matplotlib inline PNG now pass real-kernel checks. Other libraries below remain validation candidates; native tables and interactive plots are not yet implemented.

| Order | Slice / examples | Acceptance |
| --- | --- | --- |
| 1 | Images: IPython.display.Image, Pillow-generated PNG/JPEG | Real saved/live MIME images, dimensions, transparency, DPI/color, full-resolution decode/cache, large-image navigation, supported-alternative selection |
| 2 | Graphs: Matplotlib; then Plotly and Bokeh from the visual reference | Real generated figures with labels/legends/multiple plots; distinguish static images from native pan/zoom, hover or live data updates |
| 3 | Tables: pandas DataFrame; later Styler | Native readable headers/index, types/missing values, long/wide tables, scrolling/selection/copy; preserve unsupported styling/HTML and state the supported subset |
| 4 | Embedded dashboards/windows: ipywidgets, Plotly/Bokeh interactive views, iframe/server-style output | Choose an architecture consistent with the native/no-browser direction; define event/lifecycle/communication support and validate actual interactions |

Static graphs can build on the image renderer; they do not complete interactive graph parity. pandas plain text is a fallback, not native table support. The current visualization fixture contains saved placeholder text and does not certify that any library executes or renders correctly. Add version-recorded real notebooks for each library/output slice.

Arbitrary browser-dependent dashboards are unresolved under the no-browser constraint. Keep them on the roadmap at lower priority; do not silently drop the request or introduce a browser engine as an incidental dependency.

## Later breadth and platform coverage

Complete scoped Markdown/math, image/vector formats, richer output updates, syntax assistance, search, variables, export, accessible input, recovery, Explorer/installer integration, and additional Cantor backends. Test Windows versions, monitor/DPI/HDR transitions, device loss, weaker hardware, and sustained active-refresh deadlines. Keep state under the planned AppData locations.

Explicit parallel execution remains later: independent tasks, input/output ownership, cancellation, and reproducible results precede arbitrary cell concurrency. No application feature automatically makes sequential Python code multicore.

## Advancing a checkpoint

Record behavior, interfaces/ownership, relevant upstream decisions, passed tests, and limits. Use [Validation.md](Validation.md), preserve rendering invariants, update parity/status tables, and commit stable work. Numeric improvements never justify disabling live updates or reducing presentation fidelity. A documentation checkpoint does not imply that planned features were implemented.

## Implemented: 0.2.2 presentation and open preparation

[Checkpoint 09](Checkpoint-09.md) adds wrapped rendered Markdown, embedded/attachment/relative PNG/JPEG images, default viewport fitting and double-click full-size scrolling. Source pixels, notebook JSON and live resize behavior are retained. The copied private-notebook benchmark includes all supported images before publication; worker-side GPU preparation avoids moving upload work into its first scroll. Further formats, full Markdown/math/tables, low-memory behavior and the complete frame deadline remain separate acceptance work.

## Publication preparation (2026-10-16)

Build outputs now live under `Out/`; the latest Windows x64 installer and checksum live under `Dist/x64/Win/`. Original work is MIT, with dependency/font/WiX exceptions retained in [third-party notices](../THIRD-PARTY-NOTICES.md). Public documentation and historical evidence are anonymized. This is packaging/documentation work; existing feature and runtime acceptance gaps remain. See [Publication.md](Publication.md).

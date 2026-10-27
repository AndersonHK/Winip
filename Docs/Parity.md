# Cantor parity and remaining scope

Current release: **0.3.3 — Windows x86-64**. Code/Markdown editing, persistent
Python execution, confirmed environment setup, PNG/JPEG and static Matplotlib
output are implemented. Native tables, additional execution ranges and interactive
output remain planned. Embedded dashboards have lowest priority, with architecture
unresolved under the no-browser direction. See [the roadmap](Roadmap.md).

Immediate sequence: the first editing checkpoint is implemented; Hello World and selected/all execution now work; interpreter selection, cooperative interrupt/restart, PNG/JPEG and static Matplotlib output now work in 0.2.0. Next add native tables, execution ranges and interactive output. Embedded dashboards are requested at lowest priority; architecture is unresolved under the no-browser direction. See [the staged roadmap](Roadmap.md).

The long-term target is feature parity with Cantor, with a classic Jupyter-style document interface. This target is broader than the first Python-only release. The categories below come from [Cantor's project overview](https://cantor.kde.org/) and the session requirements; each requires a concrete acceptance suite before being called complete.

| Area | Current state | Remaining acceptance work |
| --- | --- | --- |
| Worksheet/document UI | Code/Markdown editing, insert/delete, undo/redo, modal shortcuts, text clipboard commands, title rename, draggable cell overflow and margin-aware animated notebook navigator | IME/grapheme/clipboard validation, accessibility, search, richer cell operations |
| Notebook persistence | nbformat 4 reading and snapshot saves; >4 GiB roundtrip verified in 0.2.1; visible operation errors; unknown fields/attachments/outputs preserved; external semantic changes refused | Disk-backed/paged documents beyond the current RAM-backed model, large-cell editing, close/session recovery, exact whitespace preservation, full crash/power-loss certification and race-free external-edit coordination |
| Rich text and mathematics | Native MD4C preview on deselection: wrapped prose, headings, emphasis, lists, code, quotes, link labels and local/embedded PNG/JPEG images | Math, tables, clickable links, remote images and complete Markdown fidelity |
| Syntax assistance | Open-source monospace text | Syntax highlighting, completion, help, diagnostics |
| Python execution | Discovery/readiness, confirmed dependency setup, retained startup diagnostics, real ipykernel, selected/all execution, cooperative interrupt, restart, force-stop/reconnect and fresh per-document counters | Run above/below, graceful shutdown, stdin, idle-crash detection and stronger backpressure |
| Other Cantor backends | None | Julia, KAlgebra, Lua, Maxima, Octave, Qalculate, R, Sage, Scilab; verify current backend behaviors before implementing adapters |
| Variables | None | Name/type/value/size inspection, editing, refresh semantics |
| Plots and rich output | PNG/JPEG saved/live images, ordered text/image blocks, static Matplotlib PNG, ICC/SDR/HDR GPU path, fit/full-size toggle and bounded eager GPU preparation on open | Additional formats/high-depth images, tables, post-idle updates and interactive native plot controls |
| Tables | Plain text alternatives only | pandas DataFrame native table display, wide/long tables, types/index, copy/selection; later scoped Styler support |
| Widgets and dashboards | Unsupported-output indication; original data retained | Lower-priority embedded interactivity, comms/lifecycle, and explicit resolution of browser-dependent output |
| Export | None | PDF, LaTeX, plain text, notebook interchange |
| Assistants and extensibility | None | Common mathematical task dialogs, backend adapters, extension contracts |
| Script editing | None | Larger-source workflow with editing/help/completion |
| Worksheet retrieval/sharing | None | Define local/remote integration and user-authorized sharing flows |
| Explicit parallel work | None | Worker/task model, parameters, progress, cancellation, results and failure handling |
| Windows integration | Native dialogs, automatic MSI prerequisite guidance, optional `.ipynb` registration, dedicated persisted Settings menu, Roaming state and versioned MSI maintenance/uninstall | Complete lifecycle/rollback/Explorer acceptance, drag/drop, multi-monitor and OS matrix |
| Rendering/performance | Hardware renderer, HDR/SDR, full-resolution GPU text cache, bounded probes | All gates in the rendering contract, complete frame/GPU traces, sustained workloads |

No browser engine or JavaScript frontend is planned. Saved JavaScript is never executed by the reader. Where a rich output depends on browser scripting, preserve its original representation and specify a native supported equivalent; a missing implementation is not parity.

This table describes long-term product requirements. A released checkpoint does not establish full Cantor parity or universal platform/performance acceptance.

## Publication preparation (2026-10-16)

Build outputs now live under `Out/`; the latest Windows x64 installer and checksum live under `Dist/x64/Win/`. Original work is MIT, with dependency/font/WiX exceptions retained in [third-party notices](../THIRD-PARTY-NOTICES.md). Public documentation and historical evidence are anonymized. This is packaging/documentation work; existing feature and runtime acceptance gaps remain. See [Publication.md](Publication.md).

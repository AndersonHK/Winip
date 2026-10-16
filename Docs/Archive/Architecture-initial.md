> Archived initial proposal. Superseded by [the current architecture](../Architecture.md). Historical wording is not the current implementation plan.

# Preliminary architecture

Original design status: **proposal**. The first [implementation checkpoint](../Checkpoint-01.md) now uses hardware Direct2D/DirectWrite and D3D11 instead of the preliminary Vulkan path. Read the [rendering contract](../Rendering-Contract.md) for mandatory current invariants. Other sections below remain planning context where they do not describe implemented behavior.

Original proposal: Windows x64, C++, and Vulkan are the working direction. Minimum Windows version, Vulkan baseline, dependency choices, and exact module boundaries remain unresolved.

## Components and responsibilities

| Component | Proposed responsibility |
| --- | --- |
| Win32 application shell | Window lifetime, native dialogs, Explorer paths, clipboard, focus, DPI, and installation integration. |
| Notebook document model | Cells, source, attachments, outputs, metadata, edits, undo/redo, and save/recovery state. |
| Layout and Vulkan renderer | Visible-cell layout, text and image caches, selection, scrolling, and output presentation. |
| Kernel controller | Select and launch Python, communicate with its IPython kernel, track execution, interrupt/restart, and recover from failure. |
| Background work | File parsing, output decoding, kernel traffic, and expensive document preparation. |
| Storage service | Resolve Windows application directories and own settings, caches, recovery, logs, and connection files. |
| Later parallel-execution service | Dispatch explicit tasks to workers using the selected Python installation. |

The document model must not depend on GPU resources or a running Python process. Saved contents should be viewable before kernel startup. Kernel failure should leave the document editable and recoverable.

## Rendering and responsiveness

Vulkan provides explicit control over rendering but is not, by itself, a performance guarantee. Layout, text processing, synchronization, allocations, and output volume require budgets too.

Proposed principles:

- Render only visible cells and an appropriate nearby margin; avoid relaying out the entire notebook on every scroll.
- Cache text layout, glyph resources, and decoded output textures; invalidate only affected content.
- Process expensive work away from the UI thread and publish bounded updates to it.
- Batch incoming output and apply backpressure so a printing kernel cannot create an unbounded UI work queue.
- Distinguish limiting displayed output from discarding stored output; make any truncation policy explicit.
- Wake for input, document changes, caret/animation deadlines, or completed background work; avoid continuous rendering when idle.
- Define handling for resize, minimize/restore, DPI changes, and graphics-device failure without losing document edits.

Measure release builds with representative notebooks before promising numeric startup, memory, or frame-time targets.

## Text editing and Windows integration

Text layout is a major subsystem. The editor needs correct cursor movement, selection, font fallback, Unicode shaping, clipboard operations, composition input, keyboard navigation, and undo across cell operations.

Evaluate DirectWrite for shaping, layout, metrics, and hit testing while retaining custom Vulkan rendering. DirectWrite explicitly supports application-specific rendering; this does not imply a ready-made DirectWrite-to-Vulkan integration. Glyph rasterization, atlas policy, and synchronization need a prototype. See [DirectWrite](https://learn.microsoft.com/en-us/windows/win32/directwrite/direct-write-portal).

Evaluate Windows Text Services Framework for input-method integration and UI Automation providers for accessibility and automated interaction. These are native platform services, not browser dependencies. See [TSF](https://learn.microsoft.com/en-us/windows/win32/tsf/text-services-framework) and [UI Automation](https://learn.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32).

## Notebook persistence and output compatibility

Use the existing [nbformat specification](https://nbformat.readthedocs.io/en/stable/format_description.html). Preserve cell identifiers, metadata, attachments, execution counts, and unknown supported-version fields as appropriate, rather than reconstructing a reduced notebook from the visible UI.

Proposed initial output support: plain text, stdout/stderr, tracebacks, and PNG/JPEG images. Markdown rendering and a deliberately scoped table representation follow. SVG, mathematical typesetting, and a restricted HTML subset require explicit decisions; arbitrary HTML/CSS fidelity is not promised.

When an output contains multiple MIME representations, choose one the application supports and preserve the rest. Never execute saved JavaScript outputs. Show a useful unsupported-output indication when no supported representation exists. Full ipywidgets compatibility is outside the initial proposal.

Use a recoverable save process with a temporary sibling file and replacement appropriate to Windows semantics; keep the previous valid document on failure. These temporary document-save files are distinct from application state stored in AppData. Exact crash-consistency guarantees need failure-injection testing. Detect external changes before overwriting a notebook.

## Python kernel integration

Launch the explicitly selected Python executable with IPython's kernel in a separate process. The app owns the document and visible UI; Python owns user-code execution. Keep pip/package ownership with that interpreter. Do not silently download another Python or switch environments.

The Jupyter protocol provides shell, output, control, input, and heartbeat communication. An initial integration needs execution requests/results, busy/idle state, errors, input requests, display updates, and reliable lifecycle handling. Match outputs to their originating execution rather than whichever cell is currently selected. See [messaging specification](https://jupyter-client.readthedocs.io/en/stable/messaging.html) and [kernel architecture](https://jupyter-client.readthedocs.io/en/stable/kernels.html).

Choose between a native C++ protocol client and a small Python bridge after a bounded prototype. Neither requires a browser, JavaScript, or a Jupyter HTTP server. A native client adds protocol and process-management work; a bridge adds an internal dependency and IPC boundary. Neither is selected yet.

Handle Windows interruption explicitly: verify kernel interrupt capabilities and the required process-launch/control behavior rather than assuming Unix signals. Keep connection secrets in private runtime files and clean up owned processes/files. Separate-process execution is fault isolation, not a security sandbox for notebook code.

## Parallel execution

Multiple workers can use the same Python installation. Extra processes do not imply extra installed Python versions.

Start with an explicit task model: a function plus inputs, or a parameterized cell with declared inputs and collected outputs. Candidate backends include joblib/loky or IPython Parallel; no selection is final. See [research](../Research.md#multicore-is-already-possible-inside-notebooks).

Potential UI: worker count, parameter sweep, progress, elapsed time, memory/CPU observations, cancellation, and a result collection view. Later, schedule independent cells with explicit dependencies.

Define serialization, data-copy costs, worker namespace isolation, file side effects, result ordering, cancellation, and worker failure before exposing parallel cell execution. Do not silently parallelize ordinary cells that share a mutable namespace. Free-threaded Python is an optional later execution target, subject to package compatibility, not an initial prerequisite.

## Next structural checkpoint

The session requires one primary class per matching `.h` / `.cpp` pair, with public/private interfaces declared in headers, encapsulated coherent state, and constructor/destructor/RAII guarantees. See [checkpoint 02: next refactor](../Checkpoint-02.md#next-checkpoint--oop-refactor).

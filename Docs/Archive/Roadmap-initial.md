> Archived initial proposal. Superseded by [the current roadmap](../Roadmap.md). Historical wording is not the current implementation plan.

# Preliminary roadmap

This is a proposed order of work, not a schedule or commitment to a particular dependency. The first objective is a reliable native notebook document experience; advanced execution follows it.

Current progress is recorded in [Checkpoint 01](../Checkpoint-01.md). The renderer prototype now uses Direct2D/DirectWrite on hardware D3D11; Vulkan tasks below are retained as the original proposal, not an outstanding dependency requirement. The full [parity scope](../Parity.md) remains active.

## 0. Establish the native foundation

- Build and run the existing Visual Studio x64 scaffold.
- Decide the minimum Windows and Vulkan versions and document supported development tools.
- Prototype a Vulkan surface in the Win32 window, including resizing, DPI changes, and idle behavior.
- Prototype text layout/rendering and basic selection before committing to the editor architecture.
- Establish AppData directory resolution before any application state is written.

Exit evidence: a responsive window displaying representative code and text, with measured idle behavior and correct resize/DPI handling. Renderer choice remains reviewable if the prototype exposes a material issue.

## 1. Deliver a notebook reader

- Accept a quoted notebook path from the command line; add Open and drag-and-drop.
- Parse `.ipynb` without launching Python.
- Display code cells, basic Markdown, text/errors, and initial static image formats.
- Preserve unknown content and clearly identify unsupported outputs.
- Use visible-cell rendering for large documents.

Exit evidence: representative notebooks open without execution, scrolling remains responsive, and malformed files produce useful errors without damaging data.

## 2. Deliver editing, saving, and recovery

- Implement cell editing and structural operations with undo/redo.
- Preserve metadata, attachments, and unsupported output representations.
- Implement save, Save As, dirty-state handling, recovery, and external-change detection.
- Exercise text input, keyboard navigation, clipboard, selection, and accessibility design.

Exit evidence: edit/save/reopen preserves content; simulated interrupted saves do not destroy the previous valid file; unsaved edits can be recovered. File protection precedes feature breadth.

## 3. Connect the user's Python

- Let the user choose an existing interpreter and validate required kernel packages.
- Show saved notebook contents immediately, independently of kernel startup.
- Implement Run, Run All, Interrupt, Restart, and clear execution state.
- Capture stdout/stderr, results, errors, and supported display updates.
- Support input requests or explicitly decline them through the protocol until implemented.
- Handle unavailable interpreters, kernel crashes, and excessive output.

Exit evidence: execution reports the intended `sys.executable`; installed packages come from the intended environment; editing remains responsive during long work; stopping/restarting execution does not lose the document.

## 4. Make it suitable for everyday Windows use

- Add an installer or documented packaging flow and an opt-in `.ipynb` association compatible with Windows default-app selection.
- Validate Explorer double-click paths containing spaces and non-ASCII characters.
- Audit application writes under normal and failure conditions.
- Establish repeatable release builds, versioned dependencies, and a practical rollback/update policy.
- Test shutdown/recovery, multiple windows if supported, sustained output, and graphics lifecycle failures.
- Publish the supported output-format and notebook-compatibility boundaries.

Exit evidence: double-click, inspect, edit, run, save, close, and reopen works without a console or browser workflow. The app meets declared performance and recovery targets on chosen reference hardware.

## 5. Add explicit parallel-execution tools

- Begin with parameter sweeps or function mapping rather than arbitrary concurrent cells.
- Expose worker count, progress, cancellation, and failures.
- Make input transfer and result collection behavior visible and reproducible.
- Evaluate overhead and numerical-library oversubscription.
- Consider independent-cell scheduling only after dependencies and shared-state rules are defined.

Exit evidence: useful speedup on sufficiently large independent work, correct serial/parallel results, predictable failures, and no claim of automatic parallelization for arbitrary Python.

## Decisions to resolve during development

| Question | Decision point |
| --- | --- |
| Windows/Vulkan baseline and fallback policy | Foundation prototype |
| Text layout, glyph rendering, and editor implementation | Foundation and editing prototypes |
| Native kernel client versus Python bridge | First kernel integration |
| One kernel per document and multi-window ownership | Before multiple-document execution |
| Supported Markdown, mathematics, table, and image formats | Reader implementation |
| Notebook save fidelity and future-format policy | Persistence implementation |
| Dependency selection and distribution licensing | Before adopting/distributing dependencies |
| Numeric performance budgets and reference workloads | After baseline measurements |
| Parallel backend and state-transfer semantics | Parallel-execution prototype |

No existing-application trial or implementation milestone is implied to have been completed by this planning work.

# Checkpoint 04 — editing, Markdown preview, and notebook shortcuts

Validated on 2026-09-13 against base commit `f1e713d`. Candidate: `x64/Release/Winip-dev-09.exe`; keep its `fonts` and `licenses` folders alongside it. Build hash and evidence map (local archive: `Artifacts/archive/validation/checkpoint-04/manifest.json`; not distributed). Manual desktop acceptance belongs to the user at this checkpoint.

## Behavior

Click code or Markdown to edit, with Unicode source, selection, caret navigation, text clipboard commands, undo/redo, and full long-line access. Add code or Markdown above/below, delete cells (including the last cell), undo deletion, and convert code/Markdown kinds. Deselecting Markdown by clicking another cell/outside the editor or pressing Escape produces rich text; changing application focus alone retains the edit.

Native MD4C 0.5.2 parsing prepares immutable DirectWrite previews for headings, emphasis, lists, quotes, code, entities and link labels. Existing full-resolution GPU text caching renders them. This is a scoped first implementation: images have labeled placeholders, HTML remains literal text, and tables/math/clickable links are still upcoming. Text currently scrolls horizontally instead of wrapping. [Source provenance](../ThirdParty/md4c/README.md).

The [shortcut table](UI-Style-Guide.md#cell-editing-and-shortcuts) records the implemented Jupyter-style subset. Escape enters command mode; Enter edits; A/B insert; D,D deletes; Z/Shift+Z undo/redo; M/Y change kind; J/K or arrows select cells. Held D does not delete, and typing d in edit mode remains text. Ctrl+Enter/Shift+Enter render Markdown; code execution is still unavailable. Alt+Enter retains fullscreen.

Ctrl+S saves a coherent snapshot on the document worker. New edits made during saving remain dirty until saved themselves. Save As creates a new destination and refuses an existing file. Normal close/open asks before discarding unsaved changes. Unknown metadata, attachments, output MIME alternatives, and untouched cell source representations are preserved semantically. Temporary files are flushed and published from the same directory; detected external semantic changes refuse replacement. There remains a small check-to-replace external-edit race; exact JSON whitespace, crash recovery and power-loss behavior are not certified.

## Ownership

`Notebook` shares unchanged cell text and original JSON across immutable snapshots, with stable runtime cell IDs and content revisions distinct from presentation changes. `CellEditor` owns editing state, immutable source layouts and 128-entry history. `Markdown`, `NotebookShortcuts`, and `NotebookStorage` each have matching header/source pairs. Canonical notebook data stays available independently of disposable GPU derivatives. The broader global application/window/worker extraction remains outstanding.

Only the changed cell is cloned and shaped per text change. The cell index vector still copies/reflows, and history is bounded by entries rather than bytes. Arbitrary 1 Mi-character edits and 10,000-cell workloads have not been shown to meet the active-frame deadline. IME, full grapheme semantics, accessibility, clipboard interoperability, and close-dialog interaction need further acceptance.

## Scripted evidence

Commands and raw samples are retained in validation/checkpoint-04 (local archive: `Artifacts/archive/validation/checkpoint-04/manifest.json`; not distributed):

| Check | Result |
| --- | --- |
| Release and Debug app builds | Passed |
| Release and Debug model suites | Passed: immutable edits, Unicode, cell operations, save/reopen and external conflicts, Markdown styles/entities and modal shortcuts |
| Hardware text-cache fidelity | 160 comparisons per suite passed across SDR/HDR, DPI through 200%, fractional positions, clipping, overflow, mutations, reuse and eviction |
| Final editing probe with screenshots | Passed: click/edit, rich Markdown deselection, delete/undo, modal guards, focus preservation, insertion, save during newer edits, reopen, 200% scale and fullscreen |
| Final editing probe without readback | Passed; measured ordinary UI maximum 1.6443 ms; three two-second intervals each recorded 0 ms process CPU and zero new frames |
| Existing window/title/menu probe | Passed; ordinary UI maximum 0.5972 ms |
| Long-line probe without readback | Passed; ordinary UI maximum 0.2734 ms |
| SDR probe without readback | Passed; ordinary UI maximum 0.2816 ms |

Reproduce with `python Scripts/build.py Release --tests`, the Debug equivalent, `python Scripts/build.py Release --name Winip-dev-09`, `python Scripts/editing_probe.py Winip-dev-09` (and `--no-snapshots`), `python Scripts/interaction_probe.py Release --name Winip-dev-09 --no-snapshots`, and `python Scripts/probe.py Release --name Winip-dev-09 --long-lines --no-snapshots` / `--sdr --no-snapshots`. Editing probes use disposable fixture copies; no cell code runs.

Failed intermediate samples remain recorded. Window-focus changes initially cancelled editing; the final handler preserves it and the probe covers deactivation before typing. A later test clicked an offscreen target after caret reveal changed scroll; the probe now brings its target into view before exercising the normal hit test. One visual run passed behavioral/idle assertions but exceeded the ordinary UI gate at 6.9491 ms while readback was enabled; both the final visual and separate timing runs passed without lowering the gate. An earlier long-line idle sample included mouse/focus activity; the quiet rerun passed. These samples are not erased or treated as proof of a universal timing guarantee.

The host is an hardware GPU (model redacted) at 144 Hz, with HDR enabled for HDR runs; this does not certify sustained 160 Hz. Transition maxima in the retained navigation/SDR samples span roughly 7–17 ms, and render preparation maxima roughly 14–25 ms. Startup/resize/resource work and vsync waits are separate from ordinary input timing. This milestone preserves synchronous complete-frame geometry behavior but does not certify every transition meets the owner's one-frame allowance. No physical HDR calibration, Windows 7–11 matrix, device-loss or external-window artifact certification was added.

## Next checkpoint

Connect a kernel through the selected existing 64-bit Python and run `print("Hello, world!")` in a code cell. Show connection/busy/idle state, associate output with the originating stable cell, and preserve responsiveness. Report missing dependencies without silently changing the user's Python installation. Commit that small milestone before adding execution ranges, interrupt/restart/shutdown and rich image/plot/table output.

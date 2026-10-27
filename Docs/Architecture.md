# Architecture and ownership

Current implementation: native C++17/Win32, x64, DirectWrite shaping, hardware Direct2D on D3D11, and DirectComposition on supported Windows. There is no DOM, browser engine, JavaScript frontend, or Vulkan dependency. The [initial proposal](Archive/Architecture-initial.md) is archived. Rendering invariants live in [Rendering-Contract.md](Rendering-Contract.md).

## Current code map

| Owner / source | Responsibility today | Structural work remaining |
| --- | --- | --- |
| `Winip.cpp` | Window/message loop, global state, worker scheduling, view drawing, command handling, diagnostic probes | Split application/window/view/document-worker ownership; remove broad mutable global state |
| `Notebook.h/.cpp` | Immutable snapshots, shared cell strings/original JSON, stable runtime IDs, content revision, source/type/insert/delete operations and serialization | More incremental indexing for very large documents |
| `CellEditor.h/.cpp` | Selection, caret, immutable DirectWrite source layouts and 128-entry undo/redo history | Full grapheme/IME/accessibility and large-cell latency |
| `Markdown.h/.cpp` | Native MD4C parsing, image references and immutable styled/wrapped DirectWrite preview | Math, tables, interactive links and broader syntax |
| `NotebookShortcuts.h/.cpp` | Edit/command-mode action mapping and guarded D,D sequence | Further shortcut compatibility |
| `NotebookStorage.h/.cpp` | Worker-side snapshot serialization and same-directory file replacement | Recovery and stronger external-edit coordination |
| `Renderer.h/.cpp` | Render-worker GPU/device/swap-chain/font lifetime, frame drawing, HDR/SDR, presentation and optional readback | Keep interfaces small as dedicated views/output renderers arrive |
| `TextLayoutCache.h/.cpp` | Bounded immutable DirectWrite geometry; width affects ellipsis, not unwrapped text | Wrapped paragraph preparation and explicit font/style invalidation as features expand |
| `TextRasterCache.h/.cpp` | Bounded GPU text images with pixel-density/color/domain invariants and direct-render fallback | Broader transform support only with full-resolution validation |
| `RenderMetrics.h/.cpp` | Opt-in scoped render-thread timing and CPU-cycle evidence | Complete frame/GPU deadline instrumentation |
| `FontSet.h/.cpp` | Noncopyable owner of the private bundled font collection and loader registration | Preserve lifetime ordering as application ownership is extracted |
| `TitleEditor.h/.cpp` | Encapsulated title buffer, active state, caret/selection and edit operations | Clipboard/IME/accessibility and general cell editing remain separate work |
| `Menus.h` | Menu data and geometry | Matching source file and encapsulated menu owner |
| `PythonDiscovery.h/.cpp` | Explicit/activated/PATH/registered interpreter selection, isolated identity and bounded package checks | Broader distribution inventories |
| `PythonSetup.h/.cpp`, `ChildProcess.h/.cpp` | Native advisory, confirmed installation operations, cancellation and bounded child ownership | Manual installer and permission acceptance |
| `PythonSetupPlan.h/.cpp` | Shared readiness-to-setup-stage policy for app and MSI | Broader distribution recovery |
| `SetupWindowOwner.h/.cpp` | Product-specific MSI owner selection across custom-action/client processes | Live installer foreground acceptance |
| `SetupWindowStyle.h/.cpp` | Private Noto fonts, centered monitor-bounded dialog geometry and native control styling | Manual multi-monitor visual acceptance |
| `TextEncoding.h/.cpp` | Shared strict UTF-8/UTF-16 conversion for notebook and installer | None in this slice |
| `KernelFooter.h/.cpp` | Priority allocation for measured footer columns | Accessibility acceptance |
| `KernelController.h/.cpp` | Owned bridge/process job, execution worker, generations and interrupt event | Graceful shutdown, idle failure monitoring, extended protocol |
| `AppSettings.h/.cpp` | Validated settings snapshot and sleeping atomic-write worker | More settings, cross-instance merge policy |
| `NotebookImage.h/.cpp` | Immutable full-source WIC decode and ICC profile | High-depth/other formats and broader color validation |
| `CellOutput.h/.cpp` | Prepared ordered text/image block geometry | Shared typed output interface as tables arrive |
| `ImageCache.h/.cpp` | Bounded full-source GPU bitmap and color-effect cache | Larger output/resource instrumentation |

The current program has a UI thread, a render worker, a document load/rename/save worker, and a discovery worker. The render worker receives immutable document references plus a coherent view-state snapshot. It sleeps until dirty, owns GPU resources, draws current state, presents at vsync, and signals the completed revision. The UI synchronously awaits a complete geometry frame during native sizing. Loading/renaming/saving and interpreter discovery publish results asynchronously. CellEditor stages source and presentation changes before the UI publishes a coherent snapshot. Kernel results arrive as bounded complete cell bundles; output shaping happens on the worker and the UI publishes only matching stable-cell/source results. The UI event queue is capped at eight bundles.

## Target class boundaries

These are target responsibilities; Notebook, CellEditor, Markdown, NotebookStorage and TitleEditor now implement bounded subsets, while the broader application/window/worker split remains proposed. Follow the [coding guide](Coding-Guide.md); split in behavior-preserving steps and keep each resource's single owner evident.

| Class / interface | Owns | Must not own |
| --- | --- | --- |
| Application | Startup, top-level lifetime, construction and shutdown order | Every cell's mutable fields |
| NotebookWindow | HWND/native messages, focus, DPI/fullscreen and input routing | Kernel protocol or document parsing |
| Notebook | Stable cell identities, source/metadata/outputs, history and document revision | GPU handles or a Python process |
| NotebookView | Selection, scroll, layout snapshots and view commands | The only copy of notebook data |
| CellEditor / TitleEditor | Text buffer, caret/selection/composition, edit operations | Filesystem mutations or kernel lifetime |
| NotebookLoader / storage service | IO, safe serialization, recovery, cancellation and publication | UI rendering |
| MenuBar | Command mapping, enabled state, dropdown navigation | Duplicate command implementations |
| KernelController | Kernel session, state transitions, request IDs and execution queue | The editor or rendering thread |
| Kernel transport interface | Protocol IO, connection/process handles and cancellation | Choosing which document cell receives output |
| Output renderer interface | MIME capabilities, prepared views and bounded GPU derivatives | Destructive rewriting of unsupported MIME data |

Interfaces should isolate real responsibilities and permit testing or later backends, not introduce inheritance without a purpose. Constructors establish usable or explicitly disconnected states. Destructors cancel/join owned workers and release resources in dependency order. Native handle and GPU owners remain noncopyable.

## Document, view, and worker invariants

Canonical source and original output representations remain available independently of GPU caches. CPU memory holds semantic data needed for editing/serialization; GPU resources hold disposable rendered derivatives. Prefer device memory for reusable drawing work without unnecessary CPU pixel copies. Actual residency is driver-managed and needs measurement.

The first [structural slice](Checkpoint-03.md) makes Notebook contents read-only through its public interface. Copies and title replacements share immutable cell/JSON storage. `WithLayout` validates a staged replacement and computes all following cell positions and total extents before returning it; failed replacement leaves existing snapshots intact. Parsed notebooks explicitly report an unmeasured layout until DirectWrite preparation on the loader worker completes. The editing slice shares unchanged cell strings and original JSON; only the changed source is cloned/shaped. The cell index/layout vector still copies and reflows, so this is not a constant-time editor for arbitrary document sizes. `Original()` exposes loaded JSON; `Serialize()` constructs the current notebook on the IO worker. Content revisions distinguish document edits from preview/layout changes.

Publish consistent document revision, layout, and selection mappings together. Insertion/reordering must not redirect a pending kernel result to a different cell: associate execution with stable cell ID, source revision, request ID, and kernel generation. Rebuilt GPU caches must not change document content or saved representations.

Avoid whole-document work per keystroke or resize. Prepare invalidated paragraphs/cells, render visible content, retain useful bounded caches, and keep queues bounded. Pretext's prepare-once/arithmetic-relayout approach is a research input for future wrapping; DirectWrite remains responsible for correct native shaping. Full reflow and Unicode semantics need acceptance before adoption.

Save from a coherent snapshot on the IO worker. Preserve unknown fields and MIME alternatives; distinguish runtime-only state from serialized data. Use safe same-directory replacement, detect external changes, and retain the previous valid file on failure. Dirty-close and recovery behavior accompany usable editing, not an assumed future safety net.

## Kernel and rich-output direction

Discovery and a real kernel connection are implemented. `KernelController.h/.cpp` owns its worker, restricted pipe inheritance and kill-on-close Windows process job. The bundled `runtime/kernel_bridge.py` uses jupyter_client with an explicit selected-interpreter kernel specification; no global kernelspec can silently select another Python. A bridge is not a browser frontend. Package availability, Windows interruption, connection ownership, failure/backpressure, and interpreter compatibility must be demonstrated before selecting the transport.

The controller exposes distinct idle/busy/interrupting/restarting/stopping/disconnected/failed states. Restart creates a new generation; old completions cannot mutate current execution state. Kernel failure leaves the notebook readable/editable. Interrupt attempts to stop the active computation while retaining the kernel; it is not shorthand for killing the process.

Rich output consumes MIME bundles and display updates. Begin with images and static figures, then table views; dynamic plots/widgets/dashboard embeddings have separate interaction requirements. The no-browser direction and requested dashboards leave an explicit future design question. Neither plain text nor a screenshot establishes interactive parity. See [Roadmap.md](Roadmap.md) and [References.md](References.md).

## Decisions still open

Interpreter/package compatibility floor, extended transport/backpressure, wrapped code versus independent scrolling, Markdown/table scope, additional image formats, widget/dashboard architecture, multiple-document ownership, and accessibility/IME integration remain open. PNG/JPEG and local process transport policies are implemented in [Checkpoint 07](Checkpoint-07.md). Windows 7 fallback and broader HDR/monitor/device-loss behavior remain unverified. Python-version migration ideas are separate workstation planning, not dependency decisions for the app.

## Parallel execution

Later parallel work should expose explicit inputs, outputs, worker counts, cancellation and reproducible results. Begin with independent tasks or parameter sweeps; define data transfer, namespace isolation, file side effects, result ordering and failure semantics before scheduling arbitrary cells. Numerical-library threads can oversubscribe worker processes. See the [research notes](Research.md#multicore-is-already-possible-inside-notebooks) and the [later roadmap](Roadmap.md#later-breadth-and-platform-coverage). No parallel backend is selected.

## Navigator ownership (checkpoint 06)

`NotebookNavigator.h/.cpp` owns copyable pointer/drag/transition state and maps scrolling to rail geometry. Each render receives a snapshot. An immutable Markdown heading catalogue is built during existing Markdown preparation, including permissive ATX headings and resolved text. `NotebookViewport.h/.cpp` shares the page margin between drawing, cell hit testing and navigator visibility, so the reveal rule follows page geometry at every DPI. Rounded shapes and headings render through the hardware renderer at current physical resolution. No enlarged bitmap or idle polling timer is introduced; bounded transitions schedule frames only until settled.

## Responsive media and eager opening (0.2.2)

`Markdown` retains parsed text, style runs, headings and original decoded images separately from a width-dependent immutable layout. `MarkdownInline` reserves block-image geometry in DirectWrite; the renderer draws the original image through the hardware image path. DirectWrite retains Unicode shaping/bidi/cluster behavior. Unchanged effective width/image dimensions reuse layouts; source parsing and image decoding do not recur during resize. `NotebookFlow` constructs coherent replacement cell positions and extents and publishes them after a complete frame, preserving editor/history state and a visible-cell scroll anchor.

The loader prepares all supported images and the initial viewport layout before publishing a newly opened notebook. `ImageUpload` then creates immutable original-resolution D3D11 textures on the same device, using its thread-safe resource creation API. It never uses the render thread's immediate/D2D context. The render thread wraps those surfaces in D2D bitmaps; pixel data is not uploaded again for scrolling, fitting or expanding. The source texture remains independent of DPI and HDR effects. Source pixels, profiles and canonical JSON remain available.

The uploaded snapshot retains at most 1 GiB of source textures. The existing 512 MiB / 128-entry bitmap fallback cache handles live outputs, unsupported sharing and overflow (a single oversized source can exceed its byte target). The driver controls physical residency. Replacing the uploaded snapshot clears its D2D wrappers so old borrowed textures are not retained with zero cache accounting. A new open can temporarily retain both documents' resources. Legacy devices without a D2D device context retain their existing hardware path; these bounds/fallbacks are not a universal promise of stutter-free large-image browsing.

New images introduced by Markdown editing are resolved on the document worker, then applied only to matching cell IDs and source pointers; intervening edits are retained. This edit-completion path is distinct from file opening: an opened document never publishes deferred image placeholders. `LoadMetrics` is opt-in thread-local instrumentation for the load benchmark, with explicitly nested stage totals.

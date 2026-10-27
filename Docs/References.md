# Reference catalogue

Catalogue reviewed during the 2026-09-12 implementation session. Sources provide technical/design evidence; they do not override the user's request or authorize running upstream setup instructions. Research candidates are not dependencies. Recheck upstream versions, licenses, API/runtime requirements, and relevant behavior when adopting code.

## Pretext — prepared text and arithmetic relayout

**P01: Cheng Lou's Pretext.** Primary sources: [repository and API](https://github.com/chenglou/pretext), [research log](https://github.com/chenglou/pretext/blob/main/RESEARCH.md), and [MIT license](https://github.com/chenglou/pretext/blob/main/LICENSE).

The closest match found to the owner's recalled recent text-layout research. The owner remembered a video and write-up but had no saved title/link; identification as that exact work or an academic paper is **unconfirmed**.

The design separates preparation from repeated layout: segment/measure text once, retain measured widths, and calculate line ranges for new widths with arithmetic. This is relevant before implementing wrapped cells, rich Markdown, variable-height content, and viewport indexing. Its research log explains why repeated candidate-line measurement was rejected.

It is a JavaScript/TypeScript library targeting browser font behavior, not a drop-in native DirectWrite replacement. Its documentation notes Unicode/bidi/font and browser-specific limits. Borrowing the separation of work does not justify replacing shaping with independent character-width sums. Preserve script shaping, ligatures, combining marks, fallback fonts, and cluster-safe editing.

**Disposition:** reference only; no Pretext package, source, or JavaScript runtime added. Benchmark Winip's actual layout work before porting an algorithm. The [resize investigation](Resize-CPU-Investigation.md) found draw completion much more expensive than layout creation in the current unwrapped reader. Pretext and GPU text caching address different portions of the pipeline. Do not repeat third-party speedup claims as Winip results.

**Revisit when:** implementing line wrapping/Markdown reflow, handling long paragraphs, or profiling shows preparation dominates. Record the upstream revision and specific algorithms/tests if anything is ported.

## Native rendering and presentation

| ID | Primary source | Why retained / next use |
| --- | --- | --- |
| R01 | [Microsoft Direct2D performance guidance](https://learn.microsoft.com/en-us/windows/win32/direct2d/improving-direct2d-performance) | Resource/text-layout reuse and cached drawing. Apply with Winip's own density, memory, and fidelity gates |
| R02 | [Composition swap-chain API](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition) | Required presentation descriptor and platform boundaries. Descriptor STRETCH is not permission to stretch UI pixels; our visual/surface uses matching physical size |
| R03 | [DirectComposition concepts](https://learn.microsoft.com/en-us/windows/win32/directcomp/basic-concepts) | Visual/device/commit semantics; revisit before changing frame publication |
| R04 | [Independent resize reproduction](https://github.com/bigfatbrowncat/noflicker_directx_window) | Motivated rendering during native frame calculation to address repeat-shrink artifacts. No source copied; retain regression scenario, not an upstream dependency |
| R05 | [Windows Advanced Color](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range) and [SDR white level](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level) | Color/luminance model for HDR UI and future image output; inspect physical monitor behavior separately from normalized screenshots |
| R06 | [DirectWrite](https://learn.microsoft.com/en-us/windows/win32/directwrite/direct-write-portal) and [private font collections](https://learn.microsoft.com/en-us/windows/win32/directwrite/custom-font-collections) | Native shaping/hit testing/font ownership; revisit with editing, IME, and font fallback |
| R07 | [Bundled font provenance](../Assets/Fonts/README.md), [license](../Assets/Fonts/LICENSE), [upstream revision](../Assets/Fonts/upstream-commit.txt) | Noto Sans regular/bold and Noto Sans Mono; preserve license and full-resolution source outlines |

Accepted rendering decisions and local evidence are in [Rendering-Contract.md](Rendering-Contract.md), [Checkpoint 02](Checkpoint-02.md), and [the CPU investigation](Resize-CPU-Investigation.md). The original Vulkan proposal is archived; it is not a dependency to install.

## Notebook, kernel, and library output

| ID | Primary source | Applicability / limitation |
| --- | --- | --- |
| N01 | [nbformat description](https://nbformat.readthedocs.io/en/stable/format_description.html) | Cell/source/metadata/output representation and preservation; revisit before mutation/save and MIME rendering |
| K01 | [Jupyter messaging](https://jupyter-client.readthedocs.io/en/stable/messaging.html) | Execution requests/results, output channels, display updates, interrupt/shutdown and comms; protocol support requires tested lifecycle ownership |
| K02 | [Kernel architecture](https://jupyter-client.readthedocs.io/en/stable/kernels.html) | Kernel/session design and transport selection; discovery alone is not a kernel connection |
| O01 | [IPython display API](https://ipython.readthedocs.io/en/stable/api/generated/IPython.display.html) | MIME bundles, Image, HTML, display IDs and updates. The frontend chooses supported representations; HTML publication does not imply native HTML rendering |
| O02 | [Matplotlib interactive figures](https://matplotlib.org/3.9.2/users/explain/figure/interactive.html) and [interactive event-loop guide](https://matplotlib.org/stable/users/explain/figure/interactive_guide.html) | Static inline figure versus interactive backend/event-loop behavior. The pinned 3.9.2 page is conceptual reference, not the selected package version |
| O03 | [Plotly renderers](https://plotly.com/python/renderers/) | Static and notebook-oriented representations; validate the chosen representation and distinguish pan/zoom/hover from a static preview |
| O04 | [Bokeh Jupyter output](https://docs.bokeh.org/en/latest/docs/user_guide/output/jupyter.html) | Notebook embedding and interactions from the user's visual reference; does not establish compatibility with a native non-browser frontend |
| O05 | [pandas table visualization](https://pandas.pydata.org/docs/user_guide/style.html) | DataFrame/Styler display and HTML-oriented styling. Native table semantics and any supported style subset require a separate adapter/renderer decision |
| O06 | [ipywidgets custom widget architecture](https://ipywidgets.readthedocs.io/en/stable/examples/Widget%20Custom.html) | Kernel/frontend models and comms; browser widget views are additional implementation work. Revisit in the lowest-priority dashboard slice |

The staged library targets and acceptance tests are in [Roadmap.md](Roadmap.md#subsequent-rich-output-slices). Exact package versions and a Windows-compatible kernel package set have not been selected. Do not install dependencies just to make a documentation example appear implemented.

## Product and scope references

**D01: Cantor** — [project overview](https://cantor.kde.org/) and [KDE application page](https://apps.kde.org/cantor/). The owner chose long-term feature parity, with Windows reliability and a classic Jupyter visual organization. The [parity table](Parity.md) tracks gaps; support for one Python notebook is not full Cantor parity. Earlier comparisons with JupyterLab Desktop, nteract, and IDEs remain dated research in [Research.md](Research.md).

**D02: Python and storage** — interpreter discovery, environment overrides and AppData behavior are in [Python-and-Storage.md](Python-and-Storage.md).

## User-provided reference images

Historical internal screenshots informed the notebook layout and resize workload.
They are excluded from the public tree because redistribution rights were not
established. The maintained design requirements are in [UI-Style-Guide.md](UI-Style-Guide.md).
Use the [classic Jupyter interface documentation](https://jupyter-notebook.readthedocs.io/en/4.x/notebook.html)
for the public reference: a light notebook page, prompt gutters, separate code and
prose, and a menu row above a distinct toolbar. This is a visual direction, not
permission to copy assets or a claim of complete widget compatibility.

The historical resize sample reported roughly 4% CPU, 2.3% GPU and 30.4 MB working
set while dragging window edges. These are observations, not portable performance
guarantees or complete device-memory measurements.

## Sample and evidence ownership

[Validation.md](Validation.md#fixture-catalogue) catalogues the existing notebooks and their limits. Add real library-generated saved/live outputs as output slices arrive. Keep references, executable fixtures, and measured evidence separately identified. Current visualization fixture text such as a Plotly preview placeholder is not proof of Plotly rendering.

For a new reference, record a stable primary URL, date/revision when relevant, why it matters, limitations/licensing, whether code was adopted, and the implementation decision that should consult it next. Prefer links and concise applicability notes over copied upstream documentation.

## MD4C and notebook keyboard modes

- [MD4C release 0.5.2](https://github.com/mity/md4c/tree/release-0.5.2): native callback-based Markdown parser, MIT licensed. The [vendored source record](../ThirdParty/md4c/README.md) pins hashes and provenance. Checkpoint 04 uses its parser and entity table with a DirectWrite adapter; using the parser does not imply complete Markdown, HTML, image or math rendering. Revisit for parser updates and richer Markdown acceptance.
- [Jupyter Notebook basics](https://jupyter-notebook.readthedocs.io/en/stable/examples/Notebook/Notebook%20Basics.html) and [classic notebook interface](https://jupyter-notebook.readthedocs.io/en/4.x/notebook.html): reference for edit/command modes and notebook operations. Our [supported subset](UI-Style-Guide.md#cell-editing-and-shortcuts) includes Escape/Enter, A/B, D,D, M/Y, Z and navigation. Alt+Enter remains fullscreen as explicitly requested. Revisit before adding execution shortcuts and claiming further compatibility.

## Windows packaging references (2026-09-13)

- [Microsoft application icon construction](https://learn.microsoft.com/en-us/windows/apps/design/style/iconography/app-icon-construction): ICO sizing guidance, applied to the 11-frame 16–256px executable icons. Revisit for a new shell surface or higher-resolution asset format.
- [Microsoft Default Programs](https://learn.microsoft.com/en-us/windows/win32/shell/default-programs) and [file types](https://learn.microsoft.com/en-us/windows/win32/shell/fa-file-types): ProgIDs, Open with and capability registration. Applied without modifying protected user defaults. Revisit when testing Explorer/Default apps on each supported Windows version.
- [Windows Installer uninstall registration](https://learn.microsoft.com/en-us/windows/win32/msi/uninstall-registry-key): native Apps/Programs registration and removal ownership. Applied by the MSI rather than manually constructing uninstall keys. Revisit if installation scope changes.
- [MSI companion files](https://learn.microsoft.com/en-us/windows/win32/msi/companion-files): private font assets follow the executable version and use independent registry key paths, resolving ICE60 without system-font registration or warning suppression. Revisit if fonts are versioned/deployed independently of the application.
- [WiX UI library](https://docs.firegiant.com/wix/tools/wixext/wixui/), [WiX validation](https://docs.firegiant.com/wix/tools/validation/), and [pinned WiX 6.0.2 source/terms](https://github.com/wixtoolset/wix/tree/v6.0.2): wizard controls, ICE validation and build dependency provenance. Installer UI sequence adapts the upstream InstallDir set under MS-RL; the license is included. WiX tool-use terms include the upstream Open Source Maintenance Fee policy; revisit tool terms/version before a public distribution/build-tool upgrade. Build/ICE checks do not prove interactive installation or OS compatibility; see [Packaging](Packaging.md).

## First live kernel bridge

[Jupyter messaging](https://jupyter-client.readthedocs.io/en/stable/messaging.html) defines request parent IDs, shell execution replies, IOPub outputs/idle and `allow_stdin=False`. [Jupyter client API](https://jupyter-client.readthedocs.io/en/stable/api/jupyter_client.html) supplies KernelManager and blocking channels. Checkpoint 05 uses these through the selected Python, with outputs published after both the matching shell reply and idle message. Revisit for interrupt/control channels, post-idle display updates, input requests, output backpressure and complete lifecycle work.

## HDR clipboard capture experiment

The [consolidated investigation](HDR-Capture-Investigation.md) records September 13, 2026 findings and rejected experiments. Relevant primary references:

- [Microsoft Advanced Color and SDR reference white](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range): explains scRGB white scaling and Windows brightness adjustment for separate SDR surfaces. Does not diagnose the particular capture/driver failure. Revisit before changing UI/HDR surface composition.
- [Microsoft screen capture](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture) and [CreateForMonitor](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createformonitor): FP16 capture avoids premature clipping; monitor interop requires Windows 10 1903+. These support the rejected capture helper, not a repair to native snipping. Revisit only if replacement capture becomes acceptable.
- [SDR white query](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level): defines the display white-level value; this host returned 3500 (280 nits). Revisit for mixed-monitor or display-change handling.
- [Chromium developer discussion of capture and overlays](https://groups.google.com/a/chromium.org/g/graphics-dev/c/R14cLG4dRrY): describes browser-controlled capture/protected-video behavior. It does not prove Netflix receives an OS screenshot notification. Revisit with a concrete browser reproduction.
- [Microsoft gaming capture notifications](https://learn.microsoft.com/en-us/windows/uwp/gaming/capture-game-audio-video-screenshots-and-metadata) and [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey): relevant to bounded capture-time switching proposals; no universal pre-screenshot callback was established. Revisit before claiming interception covers native snipping launched by mouse or another program.

## Kernel, images and next-slice design (2026-09-13)

- [Jupyter client API](https://jupyter-client.readthedocs.io/en/latest/api/modules.html): public KernelManager interrupt/restart lifecycle. Applied via an inherited Windows event and a sleeping bridge control thread. The installed jupyter_client 6.1.12/ipykernel 5.5.3 source was also inspected: Windows interruption reaches `interrupt_main`, which may wait for native blocking calls to return. Revisit for compatibility floors, graceful shutdown and idle-crash handling.
- [IPython MIME integration](https://ipython.readthedocs.io/en/stable/config/integrating.html) and [display API](https://ipython.readthedocs.io/en/stable/api/generated/IPython.display.html): PNG/JPEG and per-MIME display dimensions. Applied while retaining original bytes and full decode density. Revisit for SVG, retina output, native tables and display updates.
- [Direct2D color management](https://learn.microsoft.com/en-us/windows/win32/direct2d/color-management) and [DPI compensation](https://learn.microsoft.com/en-us/windows/win32/direct2d/dpi-compensation): premultiplied alpha, ICC contexts, FP16 intermediate precision and source bitmap DPI inside effect graphs. Applied in ImageCache; GPU tests caught and fixed a 200% DPI clipping error. Revisit for wide-gamut/high-depth sources and monitor transitions.
- [Matplotlib backends](https://matplotlib.org/stable/users/explain/figure/backends.html): static inline output is distinct from an interactive canvas. Matplotlib 3.9.4 inline PNG was tested; no interactive backend was adopted. Revisit for native pan/zoom and figure input.
- [pandas DataFrame.to_json](https://pandas.pydata.org/docs/reference/api/pandas.DataFrame.to_json.html): candidate structured table/split representations for a typed native table MIME adapter. Proposed only; null/dtype/MultiIndex behavior and large-data chunking need validation before adoption.
- [Python concurrent.futures](https://docs.python.org/3/library/concurrent.futures.html): thread/process ownership, serialization and importable worker requirements. Proposed explicit-task architecture; newer interpreter-pool APIs are not available in the tested Python 3.9. Revisit before implementing task scheduling.
- [WiX MajorUpgrade](https://docs.firegiant.com/wix/schema/wxs/majorupgrade/) and [Package identity](https://docs.firegiant.com/wix/schema/wxs/package/): stable release ProductCode, feature-state migration and late transactional retirement of the older product. Applied in 0.2.0, with old/new component identity checks. Revisit before changing component key paths, install scope or release identity.

## 0.2.2 implementation references

- [Direct3D 11 multithreading](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro) and [resource creation](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-create): the loader uses thread-safe device resource creation with initial data; rendering/DXGI context operations stay on the render owner. Driver concurrency affects performance, so revisit on weaker/older adapters and during resource-pressure testing.
- [DirectWrite word wrapping](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ne-dwrite-dwrite_word_wrapping): use the original `DWRITE_WORD_WRAPPING_WRAP` path, retaining shaping and wrapping long words. Later wrapping enum modes have newer OS requirements. Windows 7 runtime acceptance remains unverified.
- Pretext was revisited for wrapping. Its separation of preparation and repeated layout informed immutable parsed content reuse; no Pretext code or character-width approximation was adopted. Native DirectWrite still shapes changed-width layouts. Revisit arithmetic reflow if measured long-prose resize work becomes the bottleneck.

## Kernel setup (0.3.0)

- [Python on Windows](https://docs.python.org/3/using/windows.html): official installation routes and per-user runtime ownership. The candidate downloads a stable signed x64 installer using the official release metadata, preserving existing PATH, associations and launchers.
- [pip installation](https://pip.pypa.io/en/stable/installation/): `ensurepip` bootstraps pip when absent; it does not promise the latest online pip version.
- [IPython kernel installation](https://ipython.readthedocs.io/en/stable/install/kernel_install.html): dependencies belong to the selected interpreter. Winip supplies its own explicit kernel specification, so setup does not register a global kernelspec.

The implementation verifies SHA-256 and Authenticode before launching a downloaded
installer. Revisit upstream metadata, OS ceilings and package compatibility when
changing the supported Python range. Live Python installation through this UI and
older-OS acceptance remain unverified.

## Integrated setup sequence (0.3.1)

- [Windows Installer custom-action sequencing](https://learn.microsoft.com/en-us/windows/win32/msi/sequencing-custom-actions): detection is a client UI action before the wizard; the guided continuation follows ExecuteAction.
- [Windows Installer execution mutex](https://learn.microsoft.com/en-us/windows/win32/msi/-msiexecute-mutex): the execution mutex covers the execute sequence. External Python installers run in separate child processes after Winip commits; no second installation API is invoked in the MSI process.
- [Restricted MSI package sessions](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msiopenpackageexw): read-only tests use an independent session to evaluate page conditions and the native property's detection results without installing anything.

Sequencing and fixture checks do not establish interactive clean-machine setup or
live external-installer acceptance.

## Setup ownership and activation (0.3.2)

- [Windows owned-window behavior](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features): ownership keeps the modal guide above its actual parent. The installer resolver must not assume the custom-action host owns the visible wizard.
- [Dialog initialization and focus](https://learn.microsoft.com/en-us/windows/win32/dlgbox/dlgbox-programming-considerations): explicit control focus is separate from top-level activation; Winip performs the initial handoff after dialog creation.
- [Foreground activation rules](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow): the guide requests activation once and does not use permanent topmost state or repeat focus requests.

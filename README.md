# Winip

Winip is a native Windows application for opening, editing, and running Jupyter
notebooks (`.ipynb`). It combines code cells, Markdown, saved outputs, and Python
execution in a dedicated desktop window, using your existing Python environment.
The interface is written in C++ and rendered with Direct2D/DirectWrite on the GPU;
it does not embed a browser or require a Jupyter web server.

Winip is early-stage software. The current release is **0.3.3 — Windows x86-64**.

## Install and get started

Installers are distributed through [GitHub Releases](https://github.com/AndersonHK/Winip/releases), separately from the source. The local release staging
folder is [Dist/x64/Win](Dist/x64/Win/README.md); generated MSI files are not
committed to Git. If no binary release is available, build and package from source.
Run the MSI, choose an installation folder, and optionally register Winip as an
`.ipynb` handler. Installation requires administrator permission; running Winip
does not. Packages are currently unsigned.

Open a notebook with **File → Open** or **Ctrl+O**. Reading and editing saved
notebooks does not require Python. Opening a notebook never runs its cells.

To execute code, select an existing 64-bit Python interpreter through
**Kernel → Select interpreter**. That interpreter needs `ipykernel` and
`jupyter_client`, plus any libraries imported by the notebook. Winip reports
missing dependencies and opens guided setup automatically, with destination
confirmation. **Kernel → Connect kernel** or Run retries setup when needed. The
MSI checks automatically and includes skippable guidance only for missing
dependencies. A working kernel does not require pip. Run a cell
with **Ctrl+Enter**, or run all code cells with **Ctrl+Shift+Enter**. Only run
notebooks whose code you trust: execution has your account's file and network access.

## What works

- Code and Markdown editing, cell insertion/deletion, undo/redo, save and Save As.
- Wrapped Markdown previews with headings, lists, emphasis, code, and local,
  attached or embedded PNG/JPEG images.
- Saved text/errors, image outputs, and static Matplotlib figures.
- A persistent Python kernel with interpreter selection, interrupt, restart,
  force stop and shared state between cells.
- High-DPI text, horizontal cell scrolling, a draggable notebook navigator,
  fullscreen, and optional HDR (off by default).
- Buffered notebook I/O, including a recorded roundtrip test larger than 4 GiB.
  Documents still reside in memory; large files need substantial RAM.

Math typesetting, Markdown tables, clickable links, native DataFrames, interactive
plots, browser widgets, stdin prompts and full Jupyter compatibility are not yet
implemented. Unsupported notebook representations are preserved when saving.
See the [feature tracker](Docs/Parity.md) and [roadmap](Docs/Roadmap.md).

## Everyday controls

Click a cell to edit it; **Escape** returns to command mode and previews Markdown.
In command mode, **A/B** insert above/below, **D then D** deletes, **M/Y** switches
Markdown/code, and **Enter** edits. **Ctrl+S** saves, **H** opens shortcut help,
and **Alt+Enter** toggles fullscreen. Double-click an image to toggle fit/full size.

**Ctrl+Shift+I** interrupts Python, **Ctrl+Shift+R** restarts it, and
**Ctrl+Shift+.** force-stops it. A restart or force stop loses the kernel's state.
The inactive kernel status column also connects without running cells. Settings
has its own menu between Widgets and Help; About identifies the running version
and platform. Settings and kernel session files live under `%APPDATA%\Winip`; notebooks remain
where you save them. See [Python and storage](Docs/Python-and-Storage.md) and the
[full shortcut list](Docs/UI-Style-Guide.md#cell-editing-and-shortcuts).

## Build from source

Use Windows x64 with Visual Studio 2022's **Desktop development with C++** workload
(v143), a Windows SDK, and Python 3 for the build scripts. Running Winip also needs
a hardware GPU supporting Direct3D feature level 10.0 or newer. There is no software
renderer. Windows 7–11 compatibility is a development target, not a tested support
matrix; recorded runtime evidence is from Windows 10.

From the repository root:

```powershell
python Scripts/build.py Release
.\Out\Bin\x64\Release\Winip.exe .\Tests\Fixtures\visualization.ipynb
```

Keep the generated `fonts`, `licenses`, and `runtime` directories beside the EXE.
Fonts are private application resources and are not installed into Windows.
The build script discovers Visual Studio with `vswhere`; no developer-specific
paths are required. `Debug` is also supported. `--name Winip-checkpoint` creates
a separately named executable for comparison runs.

| Directory | Contents |
| --- | --- |
| `Src/`, `Runtime/` | Native application and Python kernel bridge |
| `Tests/`, `Scripts/` | Maintained fixtures, checks and build tools |
| `Assets/`, `ThirdParty/` | Branding, private fonts and pinned dependencies |
| `Out/Bin/x64/<configuration>/` | Runnable developer builds |
| `Out/Obj/x64/<configuration>/` | Compiler/linker intermediates |
| `Out/Tests/x64/<configuration>/` | Test executables |
| `Out/Packaging/` | Restored WiX tools, isolated payload and MSI intermediates |
| `Artifacts/` | Ignored local validation evidence and disposable test data |
| `Dist/x64/Win/` | Latest installer and checksum for distribution |

## Test and package

```powershell
python Scripts/build.py Release --tests
python Scripts/build.py Debug --tests
```

These tests include hardware GPU checks. Optional app-owned window probes need an
interactive desktop; see [Validation](Docs/Validation.md) for their commands,
dependencies and limitations. They are separate from manual UI acceptance.

MSI packaging additionally requires a .NET SDK and a one-time network restore:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/bootstrap_packaging.ps1
python Scripts/package.py
python Scripts/packaging_tests.py --ice
```

See [Packaging](Docs/Packaging.md) for WiX terms, release identity and installer
checks. No global Python installation or package changes are part of the build.

## Contributing and license

Start with the [development guide](Docs/README.md),
[coding conventions](Docs/Coding-Guide.md), and
[rendering contract](Docs/Rendering-Contract.md). Keep changes focused and include
applicable test results. When reporting a bug, include the Winip version,
reproduction steps and a small synthetic notebook; remove personal data and local
paths from logs and attachments before sharing them.

Original Winip work is licensed under [MIT](LICENSE), a permissive license that
allows use, modification and redistribution, including commercial use, while retaining the copyright and permission notice. Fonts, vendored libraries and the WiX-derived installer UI
retain their own licenses; see [third-party notices](THIRD-PARTY-NOTICES.md).

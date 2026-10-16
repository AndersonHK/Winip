# C++ structure and coding conventions

This is the authoritative guide for repository layout, source organization and code style. Apply it to new or touched code; avoid unrelated whole-repository rewrites.

## Folder and file names

Project-owned directories use **PascalCase**: `Assets/Branding`, `Assets/Fonts`,
`Docs/Archive`, `Installer`, `Runtime`, `Scripts`, `Src`, `Tests/Fixtures`,
`ThirdParty`, `Out`, `Artifacts` and `Dist`. Use exact casing in includes,
project files, scripts and Markdown links, even on case-insensitive Windows.
Keep architecture identifiers such as `x64`, upstream names such as
`ThirdParty/md4c`, and tool-controlled directories such as `.github`, `.vs` and
`.wix` unchanged. Installed `fonts`, `licenses` and `runtime` directories are
an existing package contract, not repository source-folder names.

Native C++ class files use matching PascalCase names. Python modules and scripts
use `snake_case.py`; PowerShell scripts use `snake_case.ps1`. Preserve upstream
filenames in vendored dependencies. Build output layout is documented in the root
README; generated tool internals do not need cosmetic renaming.

## Classes and files

- Generally one primary class per file, paired by exact case and basename: `Notebook.h` declares `Notebook`; `Notebook.cpp` implements it. Related enums, small value types, or helpers may accompany their owner. Give independently owned subsystems their own pair.
- Declare constructors, destructors, and public/private methods in the header. Keep ordinary implementations in `.cpp` files. Templates and necessary compile-time definitions are the usual header exceptions.
- Public methods express operations and observations. Private fields and helpers protect invariants; do not expose unrelated mutable fields merely to simplify callers. Avoid a global mutable state object as the long-term application architecture.
- Prefer composition and concrete value types. Introduce abstract interfaces where an actual boundary benefits—kernel transport, output renderers, storage, or independently testable platform services. Avoid a virtual hierarchy for every object. Polymorphic owners need a virtual destructor.
- PImpl may hide platform/GPU dependencies and preserve a small header. It does not replace explicit lifetime ownership or a coherent class interface.
- `Notebook`, `TitleEditor`, `FontSet`, `RenderMetrics`, `TextLayoutCache`, and `TextRasterCache` follow the paired-file direction. Menus and the application shell still contain legacy organization; migrate deliberately in the remaining structural slices.

## Ownership and state

- A successfully constructed owner is usable; otherwise construction reports failure. Destructors release resources and do not throw. Represent optional/disconnected states explicitly when they are valid operating states.
- Use `std::unique_ptr` for unique ownership, `ComPtr` for COM resources, and scoped owners for handles. Use `shared_ptr<const T>` only where immutable snapshots really cross owners/threads. Avoid owning raw pointers and implicit handle borrowing.
- Resource owners are noncopyable unless copying has a defined meaning. Enable moves only when the moved-from state and resource identity remain safe.
- The owner that starts a worker owns cancellation and joining it. Signal stop, release waits, join, then release dependencies. A thread must not outlive the data or window it accesses.
- Construct replacement document/layout/cache state before publication. Failed loads, saves, edits, or kernel transitions must leave the prior coherent document available. Pair a snapshot with its revision; discard obsolete completions.
- Cached DirectWrite layouts are immutable after publication. Changed source/font/layout inputs obtain a new layout. GPU resources are disposable derivatives; preserve canonical notebook data and original output representations for editing, saving, and device recovery.
- Keep lock scopes short. Do not perform file IO, Python execution, expensive shaping, or GPU readbacks while holding shared document/UI locks. Native resize synchronization is an explicit exception to asynchronous command completion, with its own measured path.

## Naming and formatting

Use PascalCase for classes, enums, and methods, and camelCase for local variables, parameters, and private fields, following the existing public renderer interface. Use descriptive names and units (`widthPx`, `dpi`, `elapsedMs`) where confusing pixels, DIPs, or timing domains would be costly. Give Win32 callbacks their conventional platform signatures.

Use four-space indentation, braces for multi-statement control flow, and one logical operation per line. Avoid compressed multi-function or multi-branch lines. Include the matching header first in a `.cpp`, then direct dependencies. Keep private free helpers in an unnamed namespace. Do not add `using namespace` directives to headers. Comments should explain invariants, units, ownership, or non-obvious platform behavior.

Use C++17 and the existing Visual Studio project. Add both files to the project and filters; include applicable implementations in the test project. Keep x64 guards. New Windows APIs need capability checks and a deliberate compatibility path; current build success is not Windows 7 runtime evidence.

## Script conventions

Use descriptive `snake_case` names for Python variables/functions and uppercase
module constants. Put command-line entry points behind `main()` and an
`if __name__ == '__main__'` guard. Validate arguments with `argparse`, construct
paths with `pathlib`, and use argument arrays for subprocesses. Shared Visual
Studio discovery and child-environment normalization live in
`Scripts/build_support.py`; do not copy them into additional scripts.

Use descriptive PascalCase parameters and camelCase local variables in PowerShell.
Pass paths as data with `-LiteralPath` where supported. Verify resolved targets are
inside the intended repository before recursive cleanup. Fail clearly on missing
tools; never install packages or change persistent machine configuration implicitly.

## Error handling and performance

Treat warnings as errors by default and keep build/validation logs clean. Fix the cause rather than suppressing a warning to pass a checkpoint. The application and packaging projects enable compiler/linker warnings-as-errors; WiX builds and native MSI validation use `-wx`.

Check HRESULTs and Win32 outcomes where failure affects behavior. Report errors with operation context; preserve original content and do not imply a failed operation succeeded. Do not silently switch the selected interpreter or a hardware renderer to a software fallback.

Prefer GPU drawing/composition and bounded device-resource caches when measurements justify them. The driver controls actual VRAM residency; a small Task Manager working set does not account for all device memory. Avoid redundant CPU pixel copies, but do not move editable semantic data out of its proper owner merely to lower a RAM counter.

Specify cache keys, byte/entry bounds, eviction, invalidation, and fallback before adding a cache. Include DPI, effective transforms, color/alpha, content revision, and relevant font/render settings. Unsupported cache cases must retain the direct full-resolution behavior.

See [Validation.md](Validation.md) for checkpoint checks and [Rendering-Contract.md](Rendering-Contract.md) for the authoritative presentation requirements. A performance number is not grounds to remove functionality or conceal stale output.

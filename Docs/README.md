# Winip development guide

Winip is a native C++17 Windows x64 notebook application. Start with the root
[README](../README.md) for what it does, installation and build commands. The
current application version is 0.2.2; see [Parity.md](Parity.md) for supported
features and remaining gaps.

## Authoritative guides

Each topic has one maintained source. Link to these guides instead of duplicating
rules in other documents.

| Topic | Guide |
| --- | --- |
| Product scope and requirements | [Requirements.md](Requirements.md) |
| Architecture and ownership boundaries | [Architecture.md](Architecture.md) |
| Folder/file names, code style and lifetime conventions | [Coding-Guide.md](Coding-Guide.md) |
| Physical resolution, atomic frames, HDR and rendering constraints | [Rendering-Contract.md](Rendering-Contract.md) |
| Appearance, controls, shortcuts and interaction | [UI-Style-Guide.md](UI-Style-Guide.md) |
| Test commands, interpretation, local evidence and cleanup | [Validation.md](Validation.md) |
| Documentation style and maintenance | [Writing-Guide.md](Writing-Guide.md) |
| Privacy audit, Git boundaries and publication preparation | [Publication.md](Publication.md) |
| MSI construction and installer behavior | [Packaging.md](Packaging.md) |
| Version numbering and release identity | [Releases.md](Releases.md) |
| Dependency and artwork licensing | [Third-party notices](../THIRD-PARTY-NOTICES.md) |
| Python discovery and application storage | [Python-and-Storage.md](Python-and-Storage.md) |
| Planned work and acceptance criteria | [Roadmap.md](Roadmap.md), [Parity.md](Parity.md) |
| Tables, graphs, local imports and explicit parallel execution | [Next-Slices.md](Next-Slices.md) |
| Research provenance and when to revisit sources | [References.md](References.md) |

Before changing wrapped text, read the Pretext entry in the reference catalogue.
Before extending rich output, revisit its MIME/display and widget references.
Before changing rendering, read the rendering contract and the
[resize investigation](Resize-CPU-Investigation.md).

## Historical design and validation summaries

Checkpoint summaries explain decisions and observed limits. They are not current
build instructions or portable performance certifications. Original raw reports
are kept only in ignored local archives; they are not distributed with the source.

| Checkpoint | Scope |
| --- | --- |
| [01](Checkpoint-01.md) | GPU notebook reader and renderer choice |
| [02](Checkpoint-02.md) | Menus, rename, Python discovery and resize composition |
| [03](Checkpoint-03.md) | Notebook, title editor and font ownership |
| [04](Checkpoint-04.md) | Editing, shortcuts, Markdown and persistence |
| [05](Checkpoint-05.md) | Persistent Python execution |
| [06](Checkpoint-06.md) | Floating navigator |
| [07](Checkpoint-07.md) | Kernel lifecycle, image output, settings and MSI maintenance |
| [08](Checkpoint-08.md) | Buffered large-notebook I/O |
| [09](Checkpoint-09.md) | Wrapped Markdown, image fitting and eager preparation |

The [HDR capture investigation](HDR-Capture-Investigation.md) records rejected
experiments and unresolved capture behavior. [Earlier product research](Research.md)
and the original [architecture](Archive/Architecture-initial.md) and
[roadmap](Archive/Roadmap-initial.md) are dated context, not the current design.

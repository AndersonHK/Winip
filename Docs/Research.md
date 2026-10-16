# Research findings

Research snapshot: **2026-09-12**. This was a documentation and source review, not an installation, performance benchmark, or Windows compatibility certification of the candidates.

## Existing applications

### Cantor: closest conceptual match

Cantor offers a scientific worksheet interface, Python and other computational backends, and Jupyter notebook capabilities. KDE lists Windows availability, including nightly installers explicitly intended for testing. See [Cantor's project site](https://cantor.kde.org/) and [KDE's application/download page](https://apps.kde.org/cantor/).

The owner regards Cantor as the closest existing match, but is concerned about Windows being a secondary platform and about relying on unstable builds. This is a product adoption concern, not a verified claim that every available Windows build is unstable.

Unresolved during research:

- Which supported Windows distribution, if any, satisfies the desired stability standard.
- Compatibility with an arbitrary chosen external CPython installation.
- Round-trip behavior for the owner's existing notebooks and rich outputs.
- Actual Windows storage behavior across all relevant components.

Conclusion: use Cantor as an interaction and feature reference. It has not been established as a suitable daily Windows dependency.

### JupyterLab Desktop: suitable interaction, rejected frontend technology

It supports double-clicking `.ipynb` files and selecting a custom Python executable. Its single-notebook Zen Mode hides side panels and the status bar. However, it uses Electron and an embedded JupyterLab web interface with a background server. These violate the clarified frontend requirements. Its bundled-environment defaults also require attention for a single-interpreter setup. See [launch behavior](https://github.com/jupyterlab/jupyterlab-desktop#launching-jupyterlab-desktop) and [user guide](https://github.com/jupyterlab/jupyterlab-desktop/blob/master/user-guide.md).

Application data uses AppData, but project workspace files can be written under the working directory. Configuration and workspace-directory overrides exist. The default working directory therefore matters to a clean-home setup. These are documented capabilities, not a verified no-home-writes guarantee.

Older search snippets described the project as unmaintained. The subsequently inspected release history includes renewed releases, security fixes, and removal of the maintenance warning; the older description should not be repeated as current fact. See [releases](https://github.com/jupyterlab/jupyterlab-desktop/releases).

### nteract: dedicated notebook UI, different environment model

The revived application is built with Rust/Tauri and React. Its architecture includes a background daemon and automatic environment management. See the [current project](https://github.com/nteract/nteract).

Its reviewed documentation resolves Python environments from notebook metadata, project files, or prewarmed uv/conda environments. A supported workflow that always uses an arbitrary existing system interpreter and its current pip packages was not verified. See [environment resolution](https://nteract-desktop.mintlify.app/concepts/environments) and [settings](https://nteract-desktop.mintlify.app/guides/settings).

Documented Windows application/daemon storage uses AppData. Nonetheless, Tauri uses WebView2 on Windows, so it does not satisfy the no-browser-engine frontend requirement. See [installation/storage](https://nteract-desktop.mintlify.app/installation) and [Tauri process model](https://v2.tauri.app/concept/process-model/).

### Other candidates

| Candidate | Finding and disposition |
| --- | --- |
| VS Code | Has a real visual notebook editor, including cell execution; it is not restricted to plain-text `.ipynb` viewing. Excluded because the owner rejects the general-purpose IDE and its footprint. [Documentation](https://code.visualstudio.com/docs/datascience/jupyter-notebooks). |
| PyCharm | Current unified PyCharm includes core notebooks for free and can use an existing interpreter. Excluded as a general-purpose IDE. [Product scope](https://www.jetbrains.com/help/pycharm/unified-pycharm.html). |
| Spyder | Scientific IDE with notebook-related options, but does not match the focused document-app direction. [Installation and environment options](https://docs.spyder-ide.org/current/installation.html). |
| QtConsole | Native rich console with inline output, not a replacement for editable notebook documents. [Documentation](https://qtconsole.readthedocs.io/en/stable/). |
| Satyrn | Dedicated notebook application opened from the file manager, but macOS-only. A possible design reference. [Project site](https://satyrn.app/). |
| Euporie | Notebook editing in a terminal UI, rather than the requested native Windows graphical application. [Notebook editor](https://euporie.readthedocs.io/en/stable/apps/notebook.html). |

No reviewed product was verified to meet the complete combination of native Windows UI without JavaScript, focused notebook interaction, external-Python selection, pip ownership, and comprehensive AppData storage. This is a bounded research conclusion, not proof that no such product exists.

## Multicore is already possible inside notebooks

The frontend does not impose a one-core limit on code inside a cell. Processes and suitable numerical libraries can already use multiple cores. Normal CPython's GIL restricts concurrent Python-bytecode execution in threads; free-threaded builds change that limitation but do not automatically parallelize sequential code. See [Python threading](https://docs.python.org/3/library/threading.html#gil-and-performance-considerations) and [free-threaded Python](https://docs.python.org/3.14/howto/free-threading-python.html).

Joblib's default process backend uses loky and cloudpickle, including support for interactively defined functions. Ordinary multiprocessing has importability constraints that matter in notebooks, especially with Windows process spawning. See [joblib](https://joblib.readthedocs.io/en/stable/parallel.html) and [multiprocessing](https://docs.python.org/3/library/multiprocessing.html).

IPython Parallel also provides parallel cell magics; `%%px` runs code on selected engines and does not automatically partition an arbitrary loop. See [parallel magics](https://ipyparallel.readthedocs.io/en/8.7.0/tutorial/magics.html).

Winip's opportunity is to make explicitly parallel work easier to configure, observe, cancel, and reproduce within the notebook UI.

## Precomputed text layout and resize cost

The subsequent implementation session identified Pretext as a likely match for the owner's remembered DOM-free layout research. Its prepare/layout separation is relevant to future wrapping, while current measurements point to drawing completed text as the larger cost. See [the Pretext catalogue entry](References.md#pretext--prepared-text-and-arithmetic-relayout) for primary sources, applicability, limitations, and when to revisit it; [the resize CPU investigation](Resize-CPU-Investigation.md) records the separate measured drawing optimization.

# Python selection and application storage

Winip uses a selected 64-bit Python interpreter and offers explicit installation
assistance. It does not bundle Python or create application-managed environments.
Building and testing the application never implicitly installs host packages.

## Selecting Python

Selection checks `--python <path>` first, then activated `VIRTUAL_ENV` or
`CONDA_PREFIX`, then inherited PATH for recognized terminal launches. Ordinary
launches search registered user PATH before system PATH, then PEP 514 user/machine
registrations. When that automatically selected Python lacks kernel support, an
already compatible registered environment is preferred. An invalid explicit or
activated interpreter is reported without silently selecting a different one.
Zero-byte Microsoft Store aliases are skipped; shell functions and aliases are
not inherited. Use an executable path or **Kernel → Select interpreter** for
those cases. A bounded isolated probe checks identity and architecture. A separate
bounded child imports IPython, ipykernel and jupyter_client with user-site visibility,
excluding the working directory from its module search. This can execute package
initializers and Python site hooks, but never executes notebook cells.

Reading and editing notebooks works without a kernel. Execution requires
`ipykernel` and `jupyter_client` in the chosen interpreter, plus the packages
imported by the notebook. Optional image tests use Pillow and Matplotlib. Missing
packages and broken imports are separate diagnoses. pip is not required when the
kernel dependencies already work.
The bridge runs code in the opened notebook's directory. Opening another notebook
starts a fresh kernel on the next connection or run.

## Setup and failure recovery

A selectable, scrollable setup/error advisory appears automatically after
discovery finds an incomplete environment and on explicit execution attempts or
kernel failures. X or Escape dismisses the app guide. Setup never
replays a notebook cell; **Start kernel** is a separate explicit action.

The recommended installation option is checked by default. A separate review
page requires an explicit Install action and shows the exact interpreter,
package names and destination. The default is the selected virtual/Conda environment,
or user site-packages for ordinary Python when enabled. The user can choose the
interpreter's site-packages instead. Winip invokes that exact executable's pip,
without `--upgrade`; the resolver may install dependencies required by missing
packages. If pip is absent, `ensurepip` is offered as part of this operation.
Its bundled pip is not necessarily the newest online release. Failed imports
without missing packages require repair or selection of another environment;
Winip does not guess that reinstalling unrelated packages will fix them.

When no Python is found, the user can authorize download and launch of an official
stable x64 installer. The helper reads python.org metadata, restricts download URLs
to official stable installer paths, verifies SHA-256 and the Python Software
Foundation Authenticode signature, and installs per-user with pip. It leaves PATH,
Python file associations and existing launchers unchanged. It selects the newest
available installer within the OS family ceiling; older-OS runtime compatibility
remains unverified. An official-website route is also available.

Detection repeats about once a second while installation/official-route setup is
active, with probes serialized and bounded. It stops after success, cancellation,
or a timeout. Successful installer exit without detectable Python is an error.
Only Winip-owned temporary downloads are cleaned up; shared installer caches,
Python directories and pre-existing packages are never removed as rollback.
Cancellation may leave partial external changes. Setup retains one local
`setup/last-setup.log`; kernel stderr is retained as `runtime/last-kernel-stderr.log`
under application state (the runtime directory can differ for diagnostic probes).

The MSI checks readiness automatically before showing setup choices. A compatible
environment skips dependency pages; otherwise a prominent checked installation
choice appears on a dedicated page. The guide continues within the installer UI
after Winip commits, with separate Python, pip and Jupyter steps. Skip setup remains
available. Silent MSI installation does not install Python or packages. See
[the release evidence](Checkpoint-10.md).

## Implemented storage

`AppPaths` resolves the Windows Roaming known folder, respecting redirected
profiles, and creates `%APPDATA%/Winip`. `AppSettings` stores HDR (off by default)
and open-folder policy in `settings.json` using atomic replacement. Invalid
settings fall back to defaults and are backed up before an explicit change.
Kernel connection/profile files use subdirectories of this root. Graceful
cleanup is preferred; forced termination can leave session files.

Notebooks and temporary atomic-save files remain in the directory selected by
the user. Diagnostic output paths are explicitly selected by the caller.
Uninstall preserves notebooks and per-user state. See [Packaging.md](Packaging.md).

Winip can scope environment overrides to child kernel processes. This does not
control files written by arbitrary notebook code or configure other applications.
The app must not migrate the user's Python installation as a side effect.

## Reference points

- [Python virtual environments](https://docs.python.org/3/library/venv.html): environment ownership, relevant before adding environment-management features.
- [Jupyter directories](https://docs.jupyter.org/en/latest/use/jupyter-directories.html): runtime/configuration separation, relevant before changing kernel file placement.
- [IPython configuration](https://ipython.readthedocs.io/en/stable/development/config.html): child profile behavior, relevant before changing kernel launch configuration.

These links describe upstream facilities, not promises of Winip support for
every configuration or Python version.

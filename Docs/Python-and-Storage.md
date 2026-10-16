# Python selection and application storage

Winip uses an existing 64-bit Python interpreter. It does not bundle Python,
create environments, install packages, alter persistent environment variables,
or change Python file associations.

## Selecting Python

Selection checks `--python <path>` first, then activated `VIRTUAL_ENV` or
`CONDA_PREFIX`, then inherited PATH for recognized terminal launches. Ordinary
launches search registered user PATH before system PATH. An invalid explicit or
activated interpreter is reported without silently selecting a different one.
Zero-byte Microsoft Store aliases are skipped; shell functions and aliases are
not inherited. Use an executable path or **Kernel → Select interpreter** for
those cases. The discovery probe checks identity and architecture without
executing notebook cells.

Reading and editing notebooks works without a kernel. Execution requires
`ipykernel` and `jupyter_client` in the chosen interpreter, plus the packages
imported by the notebook. Optional image tests use Pillow and Matplotlib. Package
management remains the user's responsibility; missing dependencies are reported.
The bridge runs code in the opened notebook's directory. Opening another notebook
starts a fresh kernel on the next connection or run.

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

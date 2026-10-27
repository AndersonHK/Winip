# Windows packaging

The current release is **0.3.3 — Windows x86-64**. This guide owns installer
behavior and build requirements. [Releases](Releases.md) defines version identity;
[Checkpoint 10](Checkpoint-10.md) summarizes release evidence and limits.

## Installed components and ownership

The per-machine MSI defaults to `Program Files\Winip`, with a folder picker,
optional `.ipynb` association page, Start menu shortcuts and native uninstall.
It includes the application, private fonts, kernel bridge/setup helper and license
notices. Both native executables use the static Visual C++ runtime; the target
machine does not need Visual Studio, .NET or WiX to run Winip. Notebook execution
uses a separately installed Python environment.

The association choice defaults on for fresh installations. It registers Open
with and Default apps support without replacing Windows' protected user choice.
Updates preserve the installed directory and feature state; repair preserves them
as well. No `.py` or `.ipy` association is claimed. A native callback refreshes
Explorer's association cache after the transaction.

Settings, kernel runtime state and setup logs belong to the user's Roaming Winip
folder; notebooks remain where the user saves them. Uninstall removes MSI-owned
files, shortcuts and registration, while preserving notebooks, extra user files,
Roaming state, Python and Python packages. See [Python and storage](Python-and-Storage.md).

## Automatic prerequisite setup

The client UI sequence runs read-only `DetectKernelSupport` after `CostFinalize`
and before `WelcomeDlg`. A working kernel bypasses dependency pages even if pip
is absent. Missing support receives a dedicated page with a prominent installation
choice checked by default. Skip setup or clearing that choice suppresses guidance.

After `ExecuteAction` returns and Winip is committed, `CompleteKernelSetup` opens
the shared native guide before the final completion page. It confirms the exact
interpreter, packages and destination, with separate Python/pip/Jupyter stages.
External installers run in separate child processes outside Winip's execute
transaction. Closing/skipping this guide keeps Winip installed as an editor.
Silent installation, maintenance and removal do not run these UI actions.

The guide belongs to the actual visible installer window, including when the
custom action runs in another process. It is centered and requests initial focus
once. There is no Finish-page dependency checkbox or separate app-launch action.

## Build and verify

Use the existing VS2022 C++/SDK toolchain, Python for orchestration, and .NET SDK
for the repository-local WiX 6.0.2 tool restore. Build tools and caches stay in
ignored `Out/Packaging/`; no host Python packages or persistent environment settings
are changed. WiX terms are recorded in [third-party notices](../THIRD-PARTY-NOTICES.md).

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/bootstrap_packaging.ps1
python Scripts/package.py
python Scripts/packaging_tests.py --ice
python Scripts/installer_setup_tests.py
```

The package script builds an isolated payload, checks it, then writes
`Dist/x64/Win/Winip-0.3.3-x64.msi` and `SHA256SUMS.txt`. Failed builds never replace
the distributable. `Out/Packaging/payload/` holds the runnable staging tree;
compiler objects, extracted verification files and WiX debug databases stay under
`Out/Packaging/`. Only the [distribution guide](../Dist/x64/Win/README.md) is tracked.

Compiler, linker, WiX and native ICE warnings are errors. Private fonts use MSI
companion-file versioning tied to `Winip.exe`, with stable registry key paths and
no global font installation. Stable component identities and key paths are checked
against prior packages. Add `--previous-msi <previous.msi>` for that comparison;
retain a useful baseline outside `Dist/`.

Machine installation requires administrator permission. The application runs as
the user. MSI/EXEs remain unsigned; release signing is not configured.

## Install, update and remove

A newer package offers Update without requiring a preliminary uninstall. It
installs new files before retiring the previous registration within the rollback
transaction. Opening the same release offers Repair/Remove. See [release identity](Releases.md)
for ProductCode, UpgradeCode and same-version maintenance rules.

From the repository root, silent deployment uses standard Windows Installer
properties and does not install Python/packages:

```powershell
msiexec /i .\Dist\x64\Win\Winip-0.3.3-x64.msi /qn INSTALLFOLDER="$env:ProgramFiles\Winip" ASSOCIATE_IPYNB=0
msiexec /fa .\Dist\x64\Win\Winip-0.3.3-x64.msi
msiexec /x .\Dist\x64\Win\Winip-0.3.3-x64.msi
```

Omit `ASSOCIATE_IPYNB=0` for the fresh-install default. During updates the existing
association choice is migrated. Start menu uninstall and `Uninstall.exe` delegate
to Windows Installer rather than deleting application directories directly.

## Validation boundaries

Read-only checks cover version/UI routing, opt-in/skip behavior, x64 components,
license/payload hashes, icons, native Roaming paths, upgrade identity and MSI ICE.
They do not prove a full interactive lifecycle. Use a disposable VM for fresh
install, repair, upgrade, rollback, cancellation, files-in-use, uninstall and
Explorer association acceptance:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/test_msi_lifecycle.ps1 -Msi .\Dist\x64\Win\Winip-0.3.3-x64.msi
```

Also test the guided setup under an ordinary user account, missing dependencies,
network/permission failures and supported DPI/monitor configurations. Build and
static verification results must remain distinct from those observations.
The [historical guide through 0.2.2](Archive/Packaging-through-0.2.2.md) retains
older packaging decisions and evidence references.

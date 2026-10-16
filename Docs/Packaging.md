# Windows packaging checkpoint

Current packaging layout updated 2026-10-16 for publication preparation; application version remains 0.2.2. Older acceptance sections below are explicitly historical.

## Shipped behavior

- Orange notebook/window/python icon, with 11 RGBA sizes from 16 through 256px embedded in the executable. Master and exact imagegen prompt: [branding](../Assets/Branding/README.md).
- x64, per-machine MSI. Default directory is `Program Files\Winip`; the wizard has a folder field and Browse button, then a separate notebook-association page with its checkbox checked by default. Standard progress, cancellation, repair, uninstall and downgrade protection are included. The application, private fonts and license files are inside the MSI cabinet.
- The package links the Visual C++ runtime statically. The destination computer does not need the build machine's Python, .NET SDK, WiX or Visual C++ redistributable to launch the frontend. Python notebook execution still uses an independently installed interpreter and its packages.
- `AppPaths` resolves `FOLDERID_RoamingAppData` and creates **`%APPDATA%\Winip`**, respecting redirected known folders. It is called at application startup, reports creation errors, and offers checked relative subdirectory creation for application state. `settings.json` now persists HDR (off by default) and last-opened/fixed folder policy; kernel session files also use this root. Future state writers must use this service. Notebook saves and their atomic replacement files stay beside the user-selected notebook. Explicit diagnostic paths remain caller-selected.
- The optional `.ipynb` registration supplies `Winip.Notebook`, a quoted open command, icon, Open with registration and Default apps capabilities. It does not claim `.py` or `.ipy`, which the editor cannot open as notebooks. Windows' existing user-selected default is preserved; selecting Winip as default can require confirmation in Windows. Fresh installation applies the checkbox or command-line choice. Updates migrate the installed feature state; repair retains it.
- A small MSI-embedded native callback sends `SHCNE_ASSOCCHANGED` after a successful transaction, including removal, to refresh Explorer's cached associations. It performs no registry or file writes; notification failure cannot undo an already committed installation.
- A Start menu application shortcut, a Start menu uninstall shortcut and `Uninstall.exe` are installed. The launcher discovers Winip's MSI product by its stable UpgradeCode and starts the system `msiexec /x`; Windows Installer owns confirmation, elevation, rollback, Installed apps registration and removal. There is no custom recursive delete or registry cleaner.
- Uninstall removes MSI-owned program files, shortcuts, registration values and empty installation directories. It preserves notebooks, user-created extra files and the user's Roaming state. It does not delete another application's association or Windows' protected `UserChoice` data.

## Build

Requires the existing VS2022 C++/SDK toolchain, Python for build orchestration, and .NET SDK for the pinned WiX 6.0.2 tool restore. Restore writes build tools/cache under ignored `Out/Packaging`; Python installation, packages and persistent environment are unchanged. WiX licensing is recorded in [ThirdParty/wix/LICENSE.txt](../ThirdParty/wix/LICENSE.txt); upstream tool terms are linked in the reference catalogue.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/bootstrap_packaging.ps1
python Scripts/package.py
python Scripts/packaging_tests.py --ice
```

Output: `Dist/x64/Win/Winip-0.2.2-x64.msi` and `SHA256SUMS.txt`. `Out/Packaging/payload/` contains the runnable application and uninstaller; compiler objects, extracted verification files and WiX debug databases remain under `Out/Packaging/`. The script runs read-only package checks before publishing the MSI; native ICE remains a separate service-dependent check. VERSION supplies the package and executable version. Follow the project's [0.major.minor policy](Releases.md). Same-version ProductCode is stable, so reopening that MSI offers Repair/Remove. A newer release offers an explicit Update page and keeps the previous directory and feature choices. Removal of the old product registration happens after the new files are installed, within the rollback transaction; no preliminary user uninstall is needed. Existing component identities/key paths must remain stable.

Native application/packaging builds and WiX use warnings-as-errors. The `--ice` check also passes `-wx` to native MSI validation and requires Windows Installer service access. Omit it for read-only checks in a service-restricted sandbox. The private TTF files use supported MSI companion-file versioning tied to `Winip.exe`, with stable registry key paths under `Software\Winip\PrivateFonts`; those markers are removed by MSI. This keeps fonts private and versioned with the application without fabricating language metadata or suppressing warnings.

Silent deployment uses normal Windows Installer properties:

```powershell
msiexec /i Winip-0.2.0-x64.msi /qn INSTALLFOLDER="$env:ProgramFiles\Winip" ASSOCIATE_IPYNB=0
msiexec /fa Winip-0.2.0-x64.msi
msiexec /x Winip-0.2.0-x64.msi
```

For a fresh installation, omit `ASSOCIATE_IPYNB=0` to enable registration. Updates preserve the installed feature selection; changing association state during upgrade is not currently exposed in the wizard. Machine installation needs administrator rights. MSI/EXEs are unsigned development artifacts; release signing has not been configured.

## Observed validation

- Release MSI and both native executables built successfully.
- Read-only MSI checks passed: x64 components, Program Files destination, default checkbox and both UI selection events, silent opt-out condition, ownership/quoting of association keys, native cleanup actions and transactional upgrade ordering.
- Every extracted cabinet file matches the staged runtime file by SHA256. Both icon resource groups contain the exact expected 11 RGBA PNG frames; executable version metadata exists.
- Native AppPaths checks passed: Unicode, relative-path rejection, traversal rejection, writable/reused subdirectories, file-collision failure without data loss, and actual Windows Roaming known-folder resolution. Writes in these tests use a disposable workspace folder.
- Existing Release notebook/model tests and all 160 offscreen GPU fidelity cases passed.
- Native MSI ICE validation passed with warnings treated as errors: zero errors and zero warnings. No ICEs or WiX warnings are suppressed. Recorded 0.2.0 payload/test results (local archive: `Artifacts/archive/validation/checkpoint07-packaging.json`; not distributed) and ICE result (local archive: `Artifacts/archive/validation/checkpoint07-packaging-ice.txt`; not distributed) identify the final MSI by hash.

The initial ICE run required service access outside the tool sandbox. It exposed shortcut/component issues that were corrected before the successful run.

## Remaining acceptance on a disposable Windows VM

The current host was not used for an elevated installation/uninstallation, and no desktop UI automation was performed. This checkpoint does not claim observed lifecycle, wizard layout, Explorer default-app or Windows 7–11 compatibility results. Run the supplied lifecycle script elevated in a disposable VM:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/test_msi_lifecycle.ps1 -Msi .\Winip-0.2.0-x64.msi
```

It refuses existing Winip state and checks install, custom directory, association on/off, missing-font repair, removal of program files/shortcuts/registration and the empty install folder. Logs stay beside the MSI. It has not been executed in this checkpoint.

The user handles wizard checks: Browse and Back navigation, checked/unchecked association choice, install cancellation/UAC, opening a notebook with spaces/Unicode in its name, Default apps selection, and both Settings and `Uninstall.exe` removal. Also test a higher-version upgrade, downgrade rejection, files-in-use/reboot behavior, and launch under a second non-administrator account with its own Roaming directory. Rendering/input/idle probes remain the feature session's normal checkpoint gates; packaging did not alter the renderer.

## Kernel integration review (checkpoint 05)

The runtime bridge is now copied into `runtime/kernel_bridge.py` by ad-hoc builds and explicitly enumerated by `package.py`. The application resolves it relative to its executable, independent of launch working directory; Python is launched with `-B`, so no bytecode is written beside the installed script. Kernel connection/profile files go under the existing per-user `AppPaths` root, and code runs in the opened notebook's folder. No DLL split is needed for this small stable protocol boundary yet.

Reviewed the known-folder ownership, icon resource loading, payload enumeration, static runtime packaging setting, and compile settings. Debug profile-guided optimization was removed, Debug/Release now both target C++17, and warning-as-error findings (shadowed/unused locals) were corrected. Installer build/install/upgrade testing was explicitly excluded from this feature checkpoint. The current MSI remains per-machine and requires elevation to install; the installed application itself runs as the user. Forced process termination can leave generated session files under the user's runtime directory; graceful lifecycle cleanup remains upcoming.

## 0.2.0 maintenance/update acceptance

Read-only inspection and native ICE validation pass for the final 0.2.0 package, including ProductCode/version agreement, Update/Repair wizard routing, feature migration, quoted `.ipynb` shell registration, cabinet hashes and all 11 component identities shared with 0.1.0. Existing registry key paths, component locations and GUIDs are unchanged. No live install, repair, upgrade, uninstall or file-association change was performed on the user's workstation.

Manual checkpoint:

1. Install 0.2.0 with the desired association checkbox; open a disposable notebook whose path has spaces/Unicode through Explorer's Open with menu. Windows may require choosing Winip as the default explicitly.
2. Reopen that exact MSI: choose Repair and confirm it restores missing installed assets without changing the notebook/settings or association feature choice. Use a disposable VM to remove an asset for this check.
3. On a disposable VM with 0.1.0 installed in a custom folder, open 0.2.0: the Update page should show that folder. Confirm the final product is 0.2.0, the old registration is retired, and association on/off choices and Roaming state survive.
4. Check cancellation, downgrade rejection, files in use and uninstall separately. The supplied lifecycle script covers fresh install/repair/uninstall on a disposable VM; interactive upgrade and rollback remain additional acceptance.

Program Files installation requires Windows Installer elevation. The installed application itself runs normally as the user and writes application state to Roaming Winip, not Program Files. The package remains unsigned.

## 0.2.1 large-notebook patch

`Out/Packaging/Winip-0.2.1-x64.msi` contains the buffered reader/writer and visible error dialogs. Read-only payload, icons, version/UI routing, AppPaths, native ICE and component compatibility against 0.2.0 pass; see checkpoint evidence (local archive: `Artifacts/archive/validation/checkpoint08-packaging.json`; not distributed). Existing 0.2.0 installations use the Update path, while reopening the same 0.2.1 package uses Repair/Remove. Live installation, upgrade, repair and Explorer association remain owner/disposable-VM checks, not inferred from database validation. The package is unsigned; the installed app still writes its state under the user profile.

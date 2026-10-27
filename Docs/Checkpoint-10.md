# Checkpoint 10: kernel readiness and setup

Release **0.3.3 — Windows x86-64** makes kernel readiness and recovery visible
before execution and integrates explicit installation assistance. This summary
records the design and validation boundaries; current behavior is specified in
[Python and storage](Python-and-Storage.md), [UI style](UI-Style-Guide.md) and
[packaging](Packaging.md). The original requirements are retained as an
[archived design record](Archive/Kernel-Setup-Plan-0.3.md).

## Behavior and design

- Discovery distinguishes a Python executable from usable kernel support. It
  checks IPython, ipykernel and jupyter_client in the selected interpreter. A
  working kernel does not require pip or trigger installation guidance.
- Explicit, activated and terminal selections remain fixed. Ordinary desktop
  discovery can prefer an already compatible registered interpreter rather than
  recommending changes to an incomplete default environment.
- The MSI checks automatically before its choices. Missing components receive a
  prominent default-on installation choice with Skip setup. The shared guide
  continues after Winip's transaction, with separate Python, pip and Jupyter
  stages and explicit destination confirmation.
- Package installation uses the chosen executable's pip. Python downloads are
  restricted to official stable x64 installers and checked by SHA-256 and
  publisher signature. Detection verifies completion; cleanup removes only
  Winip-owned temporary downloads, not existing interpreters or shared caches.
- Startup stderr and failure details survive the bridge boundary. Kernel actions
  reflect actual connection/busy state. The guide is centered over its real owner,
  handles MSI custom-action/client process separation, and requests focus once.
- The footer groups kernel state and Python version before document status.
  Inactive labels omit the version; clicking only their column connects without
  executing cells. Errors wrap without ellipsis, while optional columns disappear
  first at narrow widths.
- Settings has its own dropdown between Widgets and Help. About identifies the
  loaded version and platform. The navigator thumb has equal three-DIP endpoint
  margins, with drawing and drag calculations using the same rectangle.

## Evidence

| Check | Observed result and scope |
| --- | --- |
| Release and Debug model suites | Passed, including 160 text-cache and 144 image comparisons per configuration |
| Readiness fixtures | Passed missing-package, ready-without-pip, broken-import, cascading missing-dependency and invalid-selection cases in a disposable environment |
| Process and UI policy tests | Passed argument quoting, cancellation, timeout, startup stderr, owner selection, footer hit/state guards, menu extents and navigator endpoint/drag checks |
| Bootstrap fixtures | Passed release/platform selection, official URL restrictions, signature/hash rejection and success/failure cleanup using simulated processes/network |
| MSI verification | Passed native detection, ten conditional page routes, default guidance, skip/silent behavior, extracted payload hashes, upgrade identities and ICE with warnings treated as errors |
| Loaded version resource | Read-only inspection confirmed 0.3.3; About adds the Windows x86-64 target label |
| Earlier 0.3 development checks | A real kernel connected/restarted/stopped without notebook-cell execution; an official Python download passed hash/signature checks and was removed without running its installer |

User testing supplied concrete UI feedback during the 0.3 series. The initial
Finish-page opt-in was replaced by automatic detection and integrated guidance;
owner/focus handling, menu placement, footer behavior and navigator spacing were
revised accordingly. A narrow-menu measurement initially failed and passed after
compact slot sizing was corrected. These observations are not a complete GUI or
platform certification.

Explicitly requested host resets removed only named Winip/Jupyter components and
verified that Python, pip and the other environment remained installed. An initial
silent MSI uninstall required administrator elevation; the elevated retry passed.
Those operations are test preparation, not build behavior. Local inventories,
logs and reports remain under ignored `Artifacts/kernel-setup-tests/`.

## Limits

The automated owner tests use presentation doubles, so they do not prove live
Windows focus behavior. Clean-machine Python installation, every network/permission
recovery path, the complete interactive installer lifecycle, older Windows, and
multi-monitor/DPI acceptance remain separate tests. Windows 7–11 support is still
a target rather than a certified matrix. Linux and macOS builds are not implemented.

Installation cancellation can leave external changes already made by pip or the
official installer. Broken imports are reported; the guide does not guess that
reinstalling unrelated packages will repair an environment. Idle kernel-crash
monitoring, graceful shutdown and additional execution ranges remain future work.

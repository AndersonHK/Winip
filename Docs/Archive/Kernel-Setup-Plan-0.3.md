# Kernel setup design record: 0.3 series

Historical requirements completed by release 0.3.3. Current behavior is maintained
in [Python and storage](../Python-and-Storage.md), [UI style](../UI-Style-Guide.md)
and [packaging](../Packaging.md). Evidence and remaining limits are summarized in
[Checkpoint 10](../Checkpoint-10.md).

## Readiness and recovery

Discover existing Python installations in explicit/activated/PATH order and then
registered user and system locations. Keep explicit choices. Check IPython,
ipykernel and jupyter_client in the selected interpreter. A working kernel does
not need pip and must not produce package-installation advisories.

Distinguish missing Python, unsupported Python, missing packages, broken imports,
startup failure, starting, idle and busy. Show the selected executable and explain
what kernel support provides. Offer setup once after discovery and on explicit
execution attempts. Never replay notebook cells after setup. Preserve failure
details and apply the same state guards to menu and keyboard commands.

## Installation

Detection is automatic in both the MSI and application. Before its choices, the
MSI checks the environment and bypasses dependency pages when a compatible kernel
exists. Otherwise a dedicated page offers a prominent, checked-by-default Jupyter
installation choice and an explicit Skip setup action. Missing Python or pip enters
the guided sequence by default. The same native guide continues within the MSI UI
after ExecuteAction has committed Winip, before the final completion page. Python
installers run in separate child processes outside the Winip MSI transaction.

The app guide opens centered over Winip, bounded by the owner's monitor work area,
using private Noto Sans, white buttons/paper and the notebook's light-gray surround.
The install option starts checked. There is no Continue editing button: X or Escape
dismisses the app guide; closing during an operation cancels and waits for cleanup.
The installer retains Skip setup. Python, pip and Jupyter are separate steps, each
with a destination review before its Install action.

Require explicit confirmation of package names, interpreter and destination.
Prefer the selected virtual environment, otherwise user site packages. Bootstrap
missing pip only when kernel setup needs it. Do not upgrade satisfied packages.
Download and launch an official stable x64 Python installer when Python is absent;
also offer the official website. Never upgrade an existing Python unnecessarily.
Check for installation about once per second while setup is active. Verify actual
readiness after installer completion; failure to find a working Python is an error.
Remove only Winip-owned temporary downloads on failure, preserve diagnostic logs,
and offer the official route again. Never recursively remove an interpreter as
cleanup. Cancellation may leave partial external installer/package changes; report
that limitation instead of promising transactional rollback.

## Footer

Reserve measured worst-case space for finite kernel-state labels. Prioritize
errors and kernel state over document details and rendering/HDR information.
Connected kernel labels include the Python version before the next pipe separator;
inactive labels omit it. Clicking only the inactive kernel column initiates a normal
connection without running cells. Hide optional columns first as width decreases. Full errors wrap in an additional
footer row; long technical diagnostics remain accessible in the setup/details
window. Never replace a full error with an ellipsis or silently drop essential text.

## Validation intent

Keep dependency classification, automated process fixtures, package verification
and interactive acceptance distinct. Preserve failed-run diagnostics and useful
baselines. Temporary test resets must name the interpreter and components being
removed; they are not a normal build or release step. Publication includes source,
assets and reviewed summaries, with installers and raw reports kept separately.

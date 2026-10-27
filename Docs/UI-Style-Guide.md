# Notebook UI and interaction style

Visual direction: the [classic Jupyter notebook interface](https://jupyter-notebook.readthedocs.io/en/4.x/notebook.html), with a separate menu and toolbar. See [the reference catalogue](References.md#user-provided-reference-images) for scope and provenance.

## Layout and appearance

Use a light notebook page, readable dark text, pale code-cell backgrounds, and a subtle neutral page surround. HDR must preserve this intended appearance using the monitor's SDR white level. The former excessively dark presentation is a regression to avoid.

Keep the editable document title above two distinct control rows:

1. A light-gray custom menu row: File, Edit, View, Insert, Cell, Kernel, Widgets, Settings, Help. It contains fine-grained actions and dropdowns.
2. A separate white toolbar row for common actions and cell type. Its contents remain responsive at smaller widths.

There is no redundant native application menu above these rows. The native title bar/system controls are distinct from the custom application menus. Disabled future actions remain visibly disabled until they work; do not implement success-looking placeholders.

Keep a consistent prompt gutter, code/input region, output region, and visible selected-cell marker. Cell/output height must reflect measured content, never overlap following cells, and permit reaching the entire source/output. Reduce margins or adapt control slots on narrow windows. Keep menus reachable; avoid overlapping labels or hiding required content.

Current geometry in `Winip.cpp` is implementation detail: title area through 74 DIPs, menu row 74–106, toolbar 106–154, paper starting around 168. These values can change together as the layout classes are extracted. Do not treat this document as a second hard-coded layout implementation.

## Typography and assets

Use the bundled open-source Noto Sans and Noto Sans Mono fonts through the private collection. Keep the license and recorded upstream revision in [Assets/Fonts](../Assets/Fonts/README.md). Code/raw text is monospace; titles, prose, menus, and status text use the proportional family. Heading hierarchy comes from size/weight/spacing rather than excessive ornament.

Render fonts, icons, vectors, selections, carets, and decoded images at destination physical resolution. Preserve crispness at 200% and fractional DPI. Cache reuse must not enlarge lower-resolution text. Scaled test screenshots are not source assets. Always retain original image data so future views can request sufficient resolution.

## Window and navigation behavior

- Normal launch is maximized. The restored window is generously sized but bounded by the monitor work area. It remains resizable.
- Alt+Enter enters/exits fullscreen and restores the prior placement/state. Repeated keydown must not rapidly toggle it.
- Every resize, maximize, minimize, and fullscreen transaction produces coherent current geometry. No stale frame, black border, partial cell, or frozen update is an acceptable optimization.
- The title's “Click title to rename” hint sits beside the measured title. Click/F2 enters editing; Enter applies and Escape cancels. Rename preserves notebook bytes and never replaces an existing file.
- Long lines must be accessible. Current behavior uses thin cell overflow tracks plus horizontal keyboard/wheel navigation. Source lines remain horizontally scrollable; rendered Markdown wraps. A redundant native gray bottom scrollbar should not return.
- Selection and caret remain aligned with their text during scrolling and resizing. A steady caret currently permits zero idle animation work; any future animation needs explicit invalidation and a measured idle policy.

## Cell editing and shortcuts

Click code or Markdown to edit its source. Green marks edit mode; blue marks command mode. Clicking another cell or outside the cell editor, Escape, or Ctrl+M leaves editing and renders Markdown rich text. Changing application focus alone preserves the edit. Markdown previews use native full-resolution DirectWrite/GPU caches. Long source lines remain horizontally scrollable; rendered Markdown wraps to the current page width.

| Input | Behavior in this checkpoint |
| --- | --- |
| Enter (command mode) | Edit selected cell without inserting a newline |
| Escape / Ctrl+M | Leave edit mode; render Markdown |
| A / B (command mode) | Insert code above / below |
| D, D (command mode) | Delete selected cell; held-key repeats cannot delete |
| Z / Shift+Z (command mode) | Undo / redo, including cell deletion |
| M / Y (command mode) | Convert selected cell to Markdown / code |
| J/K or Down/Up (command mode) | Select next / previous cell |
| Ctrl+Z / Ctrl+Y | Undo / redo edits |
| Ctrl+X/C/V; Ctrl+A (edit mode) | Cut/copy/paste text; select source |
| Ctrl+S / Ctrl+Shift+S | Save / Save As (new destination only) |
| Ctrl+Enter / Shift+Enter | Run code or render Markdown and stay / advance |
| H (command mode) | Shortcut help |
| Alt+Enter | Fullscreen, fullscreen toggle |

This is a documented Jupyter-style subset, not complete Colab/Jupyter keybinding parity. Source is preserved during preview. Both `#test` and `# test` render as headings. Current Markdown supports headings, bold/italic/strikethrough, lists, quotes, code and link labels; PNG/JPEG data URLs, attachments and relative image paths render inline. Remote URLs and unsupported images have explicit diagnostics, raw HTML is literal, and math/tables/clickable links remain future work.

The toolbar uses separate **+ Code** and **+ Markdown** insertion buttons, **Run**, **Run all** (when space permits), and a selected-cell type dropdown. File/edit menus retain actions that do not fit the narrow toolbar. **Ctrl+Shift+Enter** runs all code cells, **Ctrl+Shift+K** connects, and **Ctrl+Shift+.** force-stops the kernel. Force stop destroys the session; it is not an interrupt. Cell overflow tracks support pointer dragging to either end.

## Floating notebook navigator

The inset vertical rail scrolls the entire document. While held, it expands unequal-height cell-density marks and Markdown heading labels around the thumb; releasing retracts them in 180 ms. A stationary held thumb stops requesting frames once expansion finishes.

Visibility follows the actual paper margin, not an arbitrary small-window breakpoint. The rail is normally visible only when its left edge clears the page's three-DIP shadow by eight DIPs. Drawing and input share `NotebookViewport::SideMargin`; with today's 1180-DIP paper and 42-DIP rail inset, that means a client width of 1286 DIPs. When there is less room, proximity within 130 DIPs left of the rail reveals it; leaving hides it, and a held drag keeps it visible. All dimensions are DPI-aware.

Only navigator headings use Noto Sans ExtraBold, reduced by two DIPs from the initial candidate (11–18 DIPs depending on proximity). They have no background plate. Notebook headings, cell text and the rest of the UI keep their existing typography. Nearby headings receive priority when labels would collide; original heading source remains intact. Each distinct trace row represents actual cell position/density, not equal spacing.

The painted thumb has a three-DIP inset from the rail on all four sides at the
scroll endpoints. Its rounded caps share centers with the rail's caps. Drawing
and dragging use the same actual thumb rectangle and travel range, so both ends
remain reachable without a jump when the thumb is grabbed.

## Kernel status and controls

The footer begins with a circle and kernel state: gray disconnected, green idle,
red busy, amber setup required/failed. Connected labels include the selected
Python version, for example `Kernel Idle — Python 3.9.7 | Saved | 51 cells`.
Stopped, disconnected and not-started labels omit the version. The document-status
column does not repeat it.

Reserve measured space for the idle/version label and finite kernel states.
Hide renderer information before document details when the window narrows.
Kernel text is not ellipsized. Errors wrap in an additional footer area; unusually
long messages can be scrolled and remain selectable in the automatic advisory.
Notebook geometry and input use the resulting viewport bounds.

Clicking only the kernel column connects a stopped, disconnected or not-yet-started
kernel without running cells. Drawing publishes the hit region; input rechecks
live state to prevent duplicate starts or restarting a running kernel. Neighboring
status fields and error rows are informational. Open menus retain ordinary
click-dismiss behavior.

Restart requires a connected kernel, Interrupt requires connected/busy, and Stop
requires an active process/request. Keyboard handlers enforce those same guards.
Run all shows a refresh symbol while busy, without continuous animation.

## Guided setup and error presentation

Missing requirements and kernel failures open the advisory automatically. Connect
or Run reopens it when recovery is still needed; there is no separate setup/details
menu item. The [Python guide](Python-and-Storage.md) owns dependency and installation
policy.

The dialog is centered over its owner and constrained to that monitor's work area.
It uses private Noto Sans, dark text, white flat buttons with gray borders, and a
white details area on the notebook's light-gray surround. Font/control geometry
is rebuilt on DPI changes. The primary action receives initial focus rather than
selecting diagnostic text.

Installer ownership must allow the visible wizard to belong to a different process
from the custom-action host. Do not attach to unrelated foreground windows. Once
shown, the guide requests normal activation and focuses its primary action once;
it is not permanently topmost and does not repeatedly reclaim focus.

The installation choice is prominent and checked by default when a component is
missing. A separate review page names the interpreter, packages and destination
before the explicit Install action. X/Escape dismisses the app guide. During an
operation, Cancel remains available; X cancels and closes after cleanup. The MSI
flow also provides Skip setup. There is no Continue editing button.

## Settings and About

Settings is a top-level dropdown between Widgets and Help. It offers HDR On/Off,
last-opened/fixed-folder mode and a native fixed-folder picker. HDR defaults off;
Downloads initializes both folder choices. Alt+S opens Settings; left/right menu
navigation and compact slots cover all nine labels at supported window widths.

Kernel → Select interpreter selects an executable for the current session.
Ctrl+Shift+I interrupts, Ctrl+Shift+R restarts and Ctrl+Shift+. force-stops.

Help → About Winip displays `Version 0.3.3 — Windows x86-64`. The numeric version
comes from the loaded executable's mapped resource, so replacing its file on disk
does not change an already-running instance's reported version. The platform label
identifies this 64-bit x86 Windows build; other platform builds must identify their
own target.

## Image sizing

Images fit the available page width and viewport height by default, without
upscaling smaller sources. Double-click switches between fitting and natural
size; vertical scrolling and the cell overflow bar expose the full image.
Output metadata dimensions remain honored, including retina images. This view
state does not dirty the notebook. Clicking an image selects its cell without
opening Markdown source; clicking text retains normal editing behavior.

# Windows 10 HDR capture investigation

September 13, 2026. **No accepted fix was found. Only documentation was committed to main; its renderer and application behavior remain unchanged.** The owner requested removal of the two experimental worktrees after preserving these findings. Experiment branches remain as source history, not accepted implementations.

## Requirement and host

Preserve the preferred live appearance with Windows HDR enabled. Native keyboard Print Screen must copy a faithful desktop image independently of the active program; the built-in Snipping Tool must also work. The investigation kept Windows HDR enabled. Replacing only Print Screen with a custom capture service does not satisfy the requirement.

Historical test conditions: Windows 10 with HDR enabled, a discrete GPU and an HDR display. The configured SDR white was 280 nits (`DISPLAYCONFIG_SDR_WHITE_LEVEL = 3500`, scRGB scale 3.5). Capture failures were observed with the Windows screenshot tools. Specific workstation device identities and unrelated desktop preferences are omitted from the public report. These observations are not a portable compatibility certification.

Browser failures were intermittent and could affect the entire window, including the toolbar. Winip supplied a consistent reproduction. No causal test established NVIDIA contrast or video enhancements as the responsible component, and no display settings were changed by the experiments.

## Results

| Test | Live appearance | Native capture / result |
| --- | --- | --- |
| Main FP16 scRGB renderer, Windows-derived white scale 3.5 | Correct, per owner | Washed out in Print Screen and snips |
| Existing app-only `--sdr` mode; Windows HDR unchanged | Exact live comparison was not separately confirmed | Captures correct, per owner |
| Vulkan packed 10-bit RGB, HDR10/PQ, same 280-nit UI white | Correct, per owner | Same washed-out capture; API/encoding change rejected as a fix |
| Vulkan HDR10 with explicit 80-nit UI white | Unacceptably dark | Print Screen brightness correct; rejected |
| FP16 Windows Graphics Capture, GPU white normalization, custom Print Screen handler | Main renderer unchanged | Owner confirmed readable capture with Winip active; yellow outline unwanted; native snipping still failed; rejected |

The custom handler was designed to work globally, but the owner-confirmed result covers Winip active only. Do not claim inactive-window acceptance from that test. Its capture session caused the Windows yellow indicator; it did not correct the native screenshot path.

## Strongest evidence and interpretation

Main renders SDR-colored UI into a 16-bit floating-point linear scRGB surface. Its color conversion multiplies decoded sRGB by the configured white scale. That is the documented adjustment needed to match Windows SDR white in an HDR surface. Its internal diagnostic export divides by the scale before sRGB encoding and looks correct.

In the supplied Vulkan screenshot, source status gray RGB 85 becomes RGB 153: `sRGB_encode(sRGB_decode(85/255) * 3.5) * 255` rounds to 153. The inspected footer had 651 exact neutral RGB 153 pixels. Lighter UI distinctions were clipped to white. The 80-nit experiment independently confirms the brightness trade-off. Together these results strongly support a capture/export white-normalization mismatch, but do not identify the exact faulty Windows, driver or destination-viewer operation. Darkening an already clipped bitmap cannot recover its lost distinctions.

The failure is not specific to FP16 versus packed RGB10, scRGB versus PQ, or DirectComposition versus the tested Vulkan presentation path. Passing GPU shader and internal export tests does not establish correct external capture.

## Correction to the game comparison

OpenRCT initially appeared to be a working native-HDR counterexample in both fullscreen Print Screen and windowed Snipping Tool. Inspection later found the normal `Documents/OpenRCT2/config.ini` selected `drawing_engine = SOFTWARE_HWD` and `enable_hdr10_output = false`. The corresponding source prefers ARGB8888 textures through accelerated SDL display; Vulkan HDR10 is separately opt-in. The owner then confirmed that changing the settings behaved as predicted and agreed OpenRCT was not a valid reference for working native-HDR captures. Do not repeat the earlier assumption that its successful captures demonstrated a working HDR10/PQ screenshot path.

The Caesar 3/likely Augustus fork's inspected source used SDL2 with 8-bit textures; its exact runtime and external HDR conversion were not verified. The owner also reports working HDR screenshots in Steam games. Whether those games use native capture, a game/platform override, another presentation path or a different reference white remains unverified. Do not assume screenshot interception merely because a game works.

## Proposed direction, not implemented

The owner observed Netflix/Chrome changing behavior when opening Snipping Tool and suggested changing Winip's rendering around a capture. A bounded experiment could intercept Print Screen or Win+Shift+S, switch the final UI presentation to an SDR surface, wait for that frame to be displayed, forward the native capture action, then restore HDR after capture/cancellation. Internal FP16 rendering can be retained. This is different from lowering white inside an HDR surface: Windows maps SDR surfaces to the configured SDR white while desktop HDR stays on.

No such switching implementation was built. Live equivalence, transition artifacts, completion/cancellation and correct timing need validation. Snipping Tool's New button, delayed captures, and other programs bypass intercepted shortcuts; watching focus/process changes may occur after the screenshot has already been acquired. No documented universal Windows 10 pre-screenshot callback was found. Gaming `AppCapture` notifications are not evidence of a universal pre-capture hook. Winip can change its own surfaces, not arbitrary other applications' rendering.

Chromium developers describe capture-related overlay/protected-video changes for browser-controlled capture. That does not prove Netflix detects native Snipping Tool or identify the cause of the owner's observed change. An always-SDR UI surface with separately managed genuine HDR content is another untested design direction. Neither proposal is an accepted unconditional system fix.

## Preserved evidence and cleanup

- Vulkan branch `experiment/vulkan-hdr10`: `95a91e0` presentation experiment, `90d5472` white-level diagnostic. Detailed record (local archive: `Artifacts/archive/hdr-capture/Vulkan-HDR10-Experiment.md`; not distributed), 80-nit result (local archive: `Artifacts/archive/hdr-capture/HDR-White-Level-Test.md`; not distributed).
- Clipboard branch `experiment/hdr-clipboard-capture`: `afca594` implementation, `83d64cd` rejection/configuration evidence. Detailed record (local archive: `Artifacts/archive/hdr-capture/HDR-Clipboard-Test.md`; not distributed), text capture probe (local archive: `Artifacts/archive/hdr-capture/validation/hdr-capture-probe.txt`; not distributed).
- Initial Windows research (local archive: `Artifacts/archive/hdr-capture/windows10-hdr-capture-research.md`; not distributed) and evolving Winip findings (local archive: `Artifacts/archive/hdr-capture/winip-hdr-capture-findings.md`; not distributed) are historical context. Their older next-step recommendations are superseded by this report.

Vulkan validation recorded 1,048 GPU color samples within one RGB10 code, notebook/model checks and 160 GPU text-cache comparisons, HDR layout/interaction/idle probes, and zero Vulkan validation warnings/errors. The clipboard experiment recorded 4,096 GPU channel comparisons at 80/203/280/1000-nit reference whites with zero byte error, successful Print Screen registration/release, and a 3840×2160 FP16 desktop capture at scale 3.5. Its probe left the clipboard unchanged. Isolated-desktop DXGI occlusion/timeouts were not counted as successful presentation tests. Broader platform/driver and mixed-monitor behavior was not certified.

Worktrees removed: `outputs/Winip-vulkan-hdr10` and `outputs/Winip-hdr-clipboard`. Their generated executables, caches and screenshots were disposable test artifacts, not committed deliverables. No private desktop screenshots were added to Git. The experiment commits remain accessible through their branches; build commands in the archived records require checking out those commits. Main receives no Vulkan renderer, 80-nit override, hotkey handler or Windows Runtime capture dependency.

Primary sources and applicability: [HDR references](References.md#hdr-clipboard-capture-experiment).

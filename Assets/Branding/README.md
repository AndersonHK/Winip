# Winip icon

`Winip.png` is the original 1254px RGBA master generated with the built-in imagegen tool on 2026-09-13. `Scripts/build_icon.ps1` converts it into `Src/Winip.ico` and `Src/small.ico`, retaining alpha and embedding 16, 20, 24, 32, 40, 48, 64, 96, 128, 192 and 256px PNG frames. Both executable icon groups use the new mark. The full-resolution master remains available for future assets.

Final generation prompt:

> Use case: logo-brand. Asset type: Windows desktop application icon for Winip, a native Python notebook program. Create one polished, simple orange app logo, no actual text or letters. A bold orange notebook shape that also reads as an application window, with a friendly minimal curled python snake motif in ivory negative space on its cover. Compact unified silhouette, rounded corners, restrained orange / amber palette, clean flat geometric design, very minimal detail and strong contrast so it remains legible at 16 pixels. Front facing, centered on a genuinely transparent background. Fill about 85 percent of the square canvas. No wordmark, no lettering, no mockup, no watermark, no surrounding scenery, no drop shadow outside the mark. Output a single square logo asset.

Rebuild with `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/build_icon.ps1`. This changes execution policy only for that process. It uses Windows System.Drawing; no Python packages are installed or required.

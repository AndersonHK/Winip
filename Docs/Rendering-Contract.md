# Rendering contract

These are mandatory product requirements. They apply to the entire UI and every future output renderer: text, icons, images, diagrams, vector geometry, selections, caret, and animations.

## Physical-resolution invariant

Render at the destination's **physical pixel resolution**. Layout may use device-independent pixels (DIPs), but rasterization must use the effective monitor DPI and all application transforms. For an axis-aligned element:

`required pixels = ceil(logical size × DPI / 96)`

A 17-DIP extent requires 34 pixels at 200% and 26 pixels at 150%. Rounding down and enlarging that raster is forbidden. Downsampling a higher-resolution representation is allowed.

- Vector shapes and font outlines must be rasterized for the destination density. Do not enlarge a previously rasterized lower-resolution surface.
- A reusable raster cache must cover the destination pixel footprint in both dimensions. Its key must account for effective scale/transform and every property that changes its pixels, including color space and relevant font/render settings. Insufficient entries must be regenerated before use at the larger scale.
- Raster image assets should use an available source representation with sufficient pixels. Upscaling is allowed **only when the source itself has insufficient resolution**. An undersized decode/cache is not an insufficient source and does not qualify for that exception.
- Preserve original source images and vector data. Decode/rasterize from the source, not from previously downscaled derivatives.
- DPI, zoom, resize, and monitor changes invalidate raster resources whose density or color representation is no longer appropriate. Preparing replacements may run asynchronously; publishing them must not produce partially updated layout or a low-resolution stretched placeholder.
- Do not implement application zoom or resize by stretching a cached screenshot of the UI.

The current renderer draws geometry and text into an exact-size native-pixel GPU back buffer. Cached DirectWrite layouts contain logical geometry; a separate bounded GPU cache retains text pixels at the exact destination density and fractional origin. `SetDpi` sets rasterization density, and unsupported cache cases draw directly. On Windows 8+, an identity DirectComposition visual presents the surface without the HWND redirection bitmap; client geometry and a complete frame are prepared during `WM_NCCALCSIZE`, before Windows applies the size change. The composition swap-chain API requires `DXGI_SCALING_STRETCH`, but there is no scale transform: allocation, client viewport, and composition clip have matching physical dimensions. The legacy HWND path uses `DXGI_SCALING_NONE` when available. Icons are loaded for the current DPI. `RasterContract.h` defines sizing/cache helpers for future outputs.

The older DXGI presentation path and OS-compositor behavior during resize still need Windows 7 testing. That compatibility path must not be claimed to satisfy this invariant until observed. PNG/JPEG output now retains full-source pixels, uses bounded GPU bitmaps, and applies GPU color management for profiled/HDR output. Initial host evidence is recorded in [Checkpoint 07](Checkpoint-07.md); [Checkpoint 08](Checkpoint-08.md) expands saved-image dimensions and documents the current GPU/RAM bounds.

## Hardware and color

A hardware GPU is required. Prefer GPU drawing, composition, resampling, and color conversion wherever the platform supports them. CPU work is reserved for document handling, text shaping/layout, and other work that cannot reasonably run on the GPU. Do not introduce WARP/reference/software-rendering fallback or full-window CPU rasterization.

HDR is a first-class presentation path, exposed as a persisted opt-in setting and OFF by default after the Windows 10 capture investigation. Use floating-point linear scRGB when the active display and presentation stack support it; use a deliberate SDR path otherwise. UI white follows the display's configured Windows SDR white level, rather than assuming that a fixed 80-nit UI is appropriate. Keep color encoding, luminance scale, alpha handling, and pixel density explicit when introducing image outputs.

## Scheduling and evidence

- The performance deadline is the active refresh interval: 6.25 ms at 160 Hz; the prior 6 ms guidance was rounded. Ordinary updates must meet the next frame; a substantial update arriving mid-frame may miss at most one.
- Native window geometry changes are synchronous and atomic. A resize handler waits for a completed full-resolution frame before finishing the transaction, pumping only sent messages needed by DXGI. Do not display partial/old-size frames to hide a timing overrun. Report native-transition wall time separately from editing/command work; waiting for vsync is not the same as CPU work.
- Synchronize composition during the native frame-calculation transaction, not after resize. Recalculate the client rectangle, rasterize a complete exact-size frame, present it, and commit the visual before completing `WM_NCCALCSIZE`. Avoid a second identical render merely because the native `WM_PAINT` follows the completed size transaction. Retaining oversized HWND buffers was tested and did not fix the reported outside-window artifacts.
- Present coherent updates at vsync without tearing or dropped frames. A vsync flag alone does not prove frame pacing.
- Expensive loading, saving, execution, decoding, and preparation run asynchronously; commit complete, bounded updates atomically.
- Opening performance is measured through completed preparation, not an earlier placeholder. Finish supported image decoding and document preparation before publishing the opened notebook. Do not defer this work into scrolling, resizing, or first interaction to improve load numbers. A longer open is preferable to later stutter; validate first-use and warm interaction costs separately.
- Sleep when unchanged. Wake only for actual input, state changes, deadlines, or completed work. No unconditional refresh loop.
- Validate small/large windows, fractional and 200%+ DPI, long cells, HDR and SDR, monitor changes, and device loss. Check physical-sized captures and pixels as well as layout.
- Measure preparation, presentation/GPU timing, input work, and idle CPU/GPU separately. Do not present a short CPU-only probe as proof of universal frame pacing or zero GPU activity.
- Functionality and presentation take precedence over optimization. Caching cannot freeze content during resizing, skip required invalidation, degrade typography, or hide unfinished updates. Validate cache misses, hits, eviction, changed content, density, color, clipping, and unsupported transforms against direct rendering.

The performance checkpoint adds bounded full-resolution GPU text images alongside shaped DIP layouts. Reuse is keyed by layout identity, DPI, fractional pixel origin, and color; display/render-setting refresh and resource-domain changes invalidate it. Non-identity transforms or oversized entries fall back to direct hardware drawing. See the [resize CPU investigation](Resize-CPU-Investigation.md) for measurement and acceptance status. Validation is recorded separately from the earlier resize-compositor checkpoint.

Platform references: [Direct2D pixel formats](https://learn.microsoft.com/en-us/windows/win32/direct2d/supported-pixel-formats-and-alpha-modes), [Windows Advanced Color](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range), [SDR white level](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level), and [private DirectWrite font collections](https://learn.microsoft.com/en-us/windows/win32/directwrite/custom-font-collections).

Composition references: [composition swap chains](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition), [DirectComposition transactions and visuals](https://learn.microsoft.com/en-us/windows/win32/directcomp/basic-concepts), and [the independent resize reproduction](https://github.com/bigfatbrowncat/noflicker_directx_window). The latter motivated the timing change; runtime acceptance remains hardware-specific.

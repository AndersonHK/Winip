# Checkpoint 06 — floating notebook navigator

Candidate `Winip-dev-14.exe`, based on `fe518d9`; binary hash and evidence (local archive: `Artifacts/archive/validation/checkpoint-06/manifest.json`; not distributed). The user's open `Winip-dev-13.exe` was left running. Manual desktop acceptance and installer testing remain user-owned.

The thick inset rail is draggable through the entire notebook. Holding it expands density marks at actual cell positions and floats Markdown headings away from the thumb. Nearby marks grow longer and nearby headings grow larger; releasing retracts the overlay. The 180 ms transition stops requesting frames when settled, even if the thumb remains held. Marks sharing a DIP row combine density; colliding heading labels prioritize proximity to the thumb.

Sidebar headings alone use bundled Noto Sans ExtraBold, two DIPs smaller than the first navigator candidate, with no white background. Other UI and notebook typography are unchanged. The added OFL font comes from the same pinned upstream revision as the existing fonts; its source and SHA-256 are recorded in the font catalogue.

Automatic visibility follows available grey page margin. `NotebookViewport` shares paper geometry between drawing, hit testing and the navigator. The rail must clear the page shadow by eight DIPs; with current geometry this means 1286 DIPs of client width. Below that, moving within 130 DIPs left of the rail reveals it; pointer exit hides it and capture keeps a held drag visible. This replaces the incorrect fixed 900-DIP breakpoint. Pixel dimensions vary with DPI.

`NotebookNavigator`, `NotebookViewport` and the existing Markdown/renderer interfaces keep matching header/source pairs. Heading text and positions are prepared with Markdown layout, and the render worker receives a coherent view-state snapshot. Rounded shapes and text render through the existing hardware pipeline at current physical resolution.

## Validation

- Release/Debug application and model builds passed. Both model suites passed 160 SDR/HDR/DPI GPU cache fidelity comparisons. Model checks include permissive Markdown heading extraction, exact scrollbar endpoints, animation settling, hover state and both sides of the paper-margin boundary.
- App-owned 72-cell notebook probe passed dragging to both ends, heading indexing, expanded/retracted appearance, 200% DPI reveal/hide, and stationary-held idle. Final timing run: maximum ordinary UI handler 0.1757 ms; all three two-second quiet intervals recorded zero frames and 0 ms measured CPU.
- Existing editing/save/Markdown probe passed on dev-13 before the geometry refinement; long-line drag and real Hello World/save/edit-during-execution probes passed on dev-14 before the pointer-leave-only correction. Ordinary UI maxima were 1.5472, 0.3289 and 0.0676 ms respectively.
- The long-line run still measured two window transitions above 6 ms (maximum 9.3233 ms). Complete synchronous rendering remains enabled; this is a recorded performance limit, not proof that every transition meets a 160 Hz deadline.
- A noninteractive run produced zero presentations and could not validate animation. Interactive runs disturbed by resize input and extra click/release events failed idle/held checks; raw failures are retained. Quiet reruns passed without weakening gates. A locked dev-13 linker output was avoided by using dev-14.

Evidence is from the hardware GPU (model redacted) / 144 Hz HDR host. GPU readbacks establish app-frame appearance, not physical HDR calibration or outside-window compositor correctness. Windows 7–11, arbitrary document sizes, all refresh rates and monitor transitions remain unverified targets. The next requested feature slice is real library-generated image output.

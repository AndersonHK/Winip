# Bundled Noto fonts

Private application fonts, distributed under the SIL Open Font License 1.1 in `LICENSE`. These are static TrueType outline fonts (not bitmap strikes or OS-installed fonts), chosen to preserve a Windows 7-compatible DirectWrite loading path.

Source: [notofonts/noto-fonts](https://github.com/notofonts/noto-fonts), archived upstream snapshot commit `ffebf8c1ee449e544955a7e813c54f9b73848eac`. Newer upstream work lives in the Noto project repositories; a future font update should deliberately choose and validate a pinned version.

| File | Upstream path | SHA-256 |
| --- | --- | --- |
| NotoSans-Regular.ttf | hinted/ttf/NotoSans/NotoSans-Regular.ttf | b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5 |
| NotoSans-Bold.ttf | hinted/ttf/NotoSans/NotoSans-Bold.ttf | c976e4b1b99edc88775377fcc21692ca4bfa46b6d6ca6522bfda505b28ff9d6a |
| NotoSansMono-Regular.ttf | hinted/ttf/NotoSansMono/NotoSansMono-Regular.ttf | d9e2b23d19f8230be7146f409a52b1d23117e635e28f2e2892cf91b7382f325b |

UI/prose use Noto Sans; code and text output use Noto Sans Mono. DirectWrite performs fallback for characters outside those fonts. Layout measurements and drawing use the same private collection and font metrics. DPI affects rasterization, not source resolution: the GPU renderer consumes the original scalable outlines at the current target density.

Sidebar navigator headings alone use Noto Sans ExtraBold from the same pinned snapshot: `hinted/ttf/NotoSans/NotoSans-ExtraBold.ttf`, SHA-256 `1cfa3aa25ee505ee66cbf12e29b13b4b8dd0f8ca9e89d08e7ca36f3788d80a57`. Existing document/UI typography is unchanged.

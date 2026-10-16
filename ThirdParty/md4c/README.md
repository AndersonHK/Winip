# MD4C provenance

Unmodified MD4C **0.5.2** source from the [pinned upstream release](https://github.com/mity/md4c/tree/release-0.5.2), under the [MIT license](LICENSE.md). Exact download URLs and SHA-256 hashes are in [upstream.json](upstream.json).

Winip compiles `md4c.c` and `entity.c` into its native Markdown adapter. The adapter creates styled DirectWrite text; no HTML renderer, JavaScript runtime, or browser engine is included. The app build copies the license to `licenses/md4c-LICENSE.md` beside the binary. Revisit the upstream parser and adapter coverage before extending Markdown features or changing parser flags.

# Source publication and privacy

The public repository contains source code, maintained fixtures, project assets,
licenses and reviewed documentation. Original Winip work uses [MIT](../LICENSE);
[third-party notices](../THIRD-PARTY-NOTICES.md) define dependency, font and
installer-UI exceptions. Generated installers are separate release assets.

## What belongs in Git

Keep source and required build inputs. Exclude IDE state, user-specific project
files, credentials, profile paths, private notebooks, raw historical test reports,
compiler intermediates and compiled executables/installers. `Out/`, `Artifacts/`
and generated files under `Dist/x64/Win/` are ignored. The distribution directory
tracks only its guide. Evidence cleanup and deduplication are defined once in
[Validation.md](Validation.md#evidence-retention-and-cleanup); folder names and
code conventions are in [Coding-Guide.md](Coding-Guide.md).

Preserve upstream attribution and copyright notices. Public author names in
third-party licenses must not be removed as though they were maintainer PII.
Screenshots or copied material without established redistribution rights belong
outside the public tree; link to public reference material instead.

## Audit before publication

Run from the repository root:

```powershell
python Scripts/audit_public.py --history --report Out/Publication/Audit.json
git -c core.ignorecase=false status --short
git diff --check
```

The scanner inventories every tracked working file and non-ignored new file,
records SHA-256/size, scans UTF-8 and UTF-16 strings, and flags common profile paths,
credentials, current account/machine names and generated private files. Its history
mode scans every reachable blob and commit. It reports categories rather than
copying sensitive matches into public output. Pattern matching is not a proof of
absence: inspect fixtures, images/metadata, upstream provenance and the extracted
installer payload too. Keep the detailed inventory and findings local.

Use the case-sensitive Git status view when reviewing case-only folder renames on
Windows. For future case-only renames, use `git mv` through a temporary name
and inspect the staged paths; the status override alone does not rename existing
index entries. This initial root snapshot is indexed from the cleaned source
without carrying forward old path entries. No persistent Git case-setting change
is needed.

## Preparation completed on 2026-10-16

The review covered all 295 files tracked at the starting revision, including IDE
binary databases, UTF-16 resources, notebooks with embedded images, vendored
sources, fonts, artwork, historical reports and project scripts. It also scanned
582 reachable historical blobs and 21 commits. Media metadata and previews,
font name/license tables, dependency hashes, and the extracted MSI payload were
inspected separately. Upstream source/font bytes were preserved.

The working tree now removes tracked IDE data, private reference screenshots and
raw historical reports. Public summaries anonymize profile/interpreter paths,
private notebook names and device identities. Local archives retain needed
historical context without adding it to Git. Build outputs use the standardized
`Out/` layout; the local installer/checksum use `Dist/x64/Win/`.

Release and Debug model suites passed with the renamed paths, including 144 image
and 160 text-cache GPU comparisons in each configuration. The Release application
and MSI built successfully. Package checks passed for payload/license hashes,
icons, version identity, installer UI routing, native AppPaths, compatibility with
0.2.1 components, and native MSI ICE with warnings treated as errors. No live
installation, upgrade, uninstall, new window interaction or real-kernel acceptance
is claimed by this maintenance pass. It changes packaging/layout/documentation,
not notebook behavior.

The local cleanup removed obsolete build trees and byte-identical generated
artifacts, with a size/hash/reason manifest in `Out/Publication/Cleanup.json`.
Private notebooks and maintained fixtures were preserved. Detailed audit and
package reports remain ignored local files, not committed evidence.

## Public history and local archive

The public `main` branch begins with one parentless commit of the cleaned source.
The original development history is preserved on the **local-only**
`private-history` branch. These experimental branches also remain local:

- `experiment/hdr-clipboard-capture`
- `experiment/vulkan-hdr10`

Continue ordinary development on `main`, tracking `origin/main`. New commits
append normally both locally and on GitHub. The archive preserves the original
history without needing duplicate copies of future public commits.

The local repository config restricts ordinary origin pushes to
`refs/heads/main:refs/heads/main`. It also disables automatic tag following and
uses the GitHub no-reply author email for future commits in this repository.
Local-only branches have no GitHub upstream. Never merge their history into
public `main`, push them explicitly, or use `--all` or `--mirror`: the private
history still contains removed IDE data, old paths and private identity metadata.

The audit script's `--history` option intentionally scans **all local refs**,
including the private archive and experiments, so historical findings remain
expected in this checkout. Verify the publication graph separately with
`git rev-list --count main` and `git rev-list --parents main`; at initial
publication there must be exactly one commit and no parent. Inspect remote heads
with `git ls-remote --heads origin` to confirm only the public branch was sent.

Before publishing any future branch, audit its files and ancestry. A clean working
tree alone does not establish that its history is safe to publish.

The installer remains an unpublished 0.2.2 candidate, with complete notices and
no build-machine PDB path. Version handling after a public release follows
[Releases.md](Releases.md). Upload the tested MSI and checksum as release assets;
do not add them to Git merely because they are located under `Dist/`.

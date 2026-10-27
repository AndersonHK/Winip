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

## Publication baseline and release checks

The 2026-10-16 publication pass removed private IDE/build data from the public
snapshot, retained required licenses/assets, and placed generated products under
`Out/`, `Artifacts/` and `Dist/`. The original development history remains local
under the archive branches described below.

For release 0.3.3, review the complete source/test/documentation change set, run
applicable [validation](Validation.md#release-033-checks), verify MSI payload/ICE
and checksum, and keep only the current release in `Dist/x64/Win/`. Write an ignored
size/hash/reason manifest before retiring old distribution files. Preserve useful
upgrade-comparison installers under `Out/Packaging/` and lightweight reports under
`Artifacts/`; do not add those files to Git.

A new Winip release must also be officially published through **GitHub Releases**.
A source commit, branch push or Git tag alone does not complete a release. Normal
development commits do not require a release; an instruction to publish a new
release includes this step. Report source publication and asset publication
separately and verify both.

## Official GitHub release workflow

Use the repository's [New release page](https://github.com/AndersonHK/Winip/releases/new)
or GitHub's [official Releases API](https://docs.github.com/en/rest/releases/releases).
Both create the same standard GitHub Release; attaching binaries to source commits
or issues is not a substitute.

1. Complete validation and documentation, commit the release source and push the
   public branch. Record the exact commit SHA; do not target a moving branch by
   assumption or include local-only archive ancestry.
2. Create a draft release with tag `v<VERSION>` at that commit. Include the version,
   OS and architecture in the title, for example `Winip 0.3.3 — Windows x86-64`.
   If a tag already exists, verify its target rather than moving it silently.
3. Write concise notes covering changes, installation requirements, validation
   and material limitations. Mark previews as prereleases; ordinary releases use
   the normal published-release state.
4. Attach the validated MSI and `SHA256SUMS.txt` from `Dist/x64/Win/` as release
   assets. Verify asset names, sizes and SHA-256 values before publishing. Keep
   binaries and raw validation reports out of Git.
5. Publish the release, then verify its public page, tag target and downloadable
   assets. For the current ordinary release, confirm GitHub marks it as latest.
   Retain a lightweight local publication receipt and report the release URL.

After publication, use a new version for changed binaries. Do not silently replace
published assets or move a release tag. Future Linux/macOS builds follow the same
workflow with explicitly named OS/architecture assets and matching checksums.

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

The current installer is **0.3.3 — Windows x86-64**, with complete notices and no
build-machine PDB path. Its MSI and checksum are separate generated release assets,
not Git source. Follow [Releases.md](Releases.md) for subsequent version changes;
never force-add installer binaries simply because they are in `Dist/`.

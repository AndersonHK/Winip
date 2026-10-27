# Release identity

Use the project early-release scheme **0.major.minor**:

- Increment `major` for substantial features or behavior changes, resetting `minor` to zero.
- Increment `minor` for fixes and patches.
- The leading zero marks the current early-stage release series; decide any version-policy change explicitly.

The current release is **0.3.3 — Windows x86-64**. It adds readiness detection,
confirmed guided setup, retained kernel diagnostics, responsive status and menu
controls, equal navigator margins, and explicit platform/version identification.
See [Checkpoint 10](Checkpoint-10.md) for changes, evidence and limitations.

The 0.3.0–0.3.2 packages were local validation iterations of this release. The
previous public source baseline is 0.2.2, which added responsive Markdown/images
and eager open preparation. Earlier checkpoints are indexed in [Docs/README](README.md).

About identifies both the loaded product version and the build target. Windows
x86-64 denotes the 64-bit x86 Windows build, not a platform-independent binary.
Future Linux or macOS builds must identify their own operating system and CPU
architecture. The target label does not change the numeric MSI/product version.

Update [VERSION](../VERSION) and [ProductVersion.rc](../Src/ProductVersion.rc) together. The build script checks they agree. `package.py` uses VERSION by default and applies it to the MSI and native executable resources. Its `--version` override is for isolated upgrade fixtures; it does not silently advance repository release identity. MSI limits the fields to 255.255.65535.

Keep the UpgradeCode fixed. `Scripts/versioning.py` derives a stable ProductCode from release version and architecture, so rebuilding the same version retains maintenance identity. A newer version gets a new ProductCode. Never distribute different release contents under an already released version: Repair restores that version's files and is not the delivery mechanism for unversioned changes. MSI PackageCode remains generated for the particular package build.

Same-version repairs used during local pre-release testing are historical
exceptions, not an update strategy for published packages. [Checkpoint 09](Checkpoint-09.md)
records the earlier 0.2.2 recache, and Microsoft's [REINSTALLMODE reference](https://learn.microsoft.com/en-us/windows/win32/msi/reinstallmode)
defines its semantics. After publishing 0.3.3, advance the version for changed binaries.

Double-clicking the installed release's MSI offers Repair/Remove. A newer MSI detects the older product and offers Update using its installed directory and migrated feature state. The update installs its files before retiring the prior product registration, within the rollback transaction. Users do not need to uninstall first. This is a Windows Installer major-upgrade mechanism; it still internally removes superseded product registration. Stable component IDs, key paths and target locations are required for this scheduling policy.

Read-only checks compare the old and new MSI component identities and verify maintenance/update UI routing. Live wizard, repair, upgrade, rollback and Explorer association behavior remain manual/disposable-VM acceptance; building and ICE validation do not establish those results. See [Packaging.md](Packaging.md).

## Distribution artifacts

Keep only source, assets, guides and concise reviewed validation summaries in Git. Build/install results, raw test reports and intermediary compiler files stay ignored. `Dist/x64/Win/` holds the latest locally built MSI and checksum for upload as release assets; only its guide is tracked. Use a new version for changes after a public release.

Every new release must have an official GitHub Release with a `v<VERSION>` tag,
release notes and verified platform-specific assets. A commit/push alone is not
release publication. Follow the [GitHub release workflow](Publication.md#official-github-release-workflow).

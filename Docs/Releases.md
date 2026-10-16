# Release identity

Until publication to GitHub, use the project scheme **0.major.minor**:

- Increment `major` for substantial features or behavior changes, resetting `minor` to zero.
- Increment `minor` for fixes and patches.
- The leading zero marks the current early-stage release series; decide any version-policy change explicitly.

The current release is **0.2.2**: responsive Markdown/images and eager open preparation. 0.2.1 added large-notebook open/save and visible error reporting fixes. The first MSI was 0.1.0; 0.2.0 added kernel lifecycle, image output and settings. `Winip-dev-24` identifies the historical validation build, not a separate product version.

Update [VERSION](../VERSION) and [ProductVersion.rc](../Src/ProductVersion.rc) together. The build script checks they agree. `package.py` uses VERSION by default and applies it to the MSI and native executable resources. Its `--version` override is for isolated upgrade fixtures; it does not silently advance repository release identity. MSI limits the fields to 255.255.65535.

Keep the UpgradeCode fixed. `Scripts/versioning.py` derives a stable ProductCode from release version and architecture, so rebuilding the same version retains maintenance identity. A newer version gets a new ProductCode. Never distribute different release contents under an already released version: Repair restores that version's files and is not the delivery mechanism for unversioned changes. MSI PackageCode remains generated for the particular package build.

During 0.2.2 validation the owner installed an unfinished local candidate and explicitly requested its replacement. The final package was recached with forced same-version file replacement using MSI `/fvamus`, followed by installed/payload hash and unchanged-settings verification. This local pre-release repair is recorded in [Checkpoint 09](Checkpoint-09.md); those hashes identify the historical package. The publication candidate is rebuilt with complete license notices and a path-independent debug reference; it remains an unpublished 0.2.2 candidate, not a repair/update for a public release. [Microsoft's REINSTALLMODE reference](https://learn.microsoft.com/en-us/windows/win32/msi/reinstallmode) documents forced replacement and recaching.

Double-clicking the installed release's MSI offers Repair/Remove. A newer MSI detects the older product and offers Update using its installed directory and migrated feature state. The update installs its files before retiring the prior product registration, within the rollback transaction. Users do not need to uninstall first. This is a Windows Installer major-upgrade mechanism; it still internally removes superseded product registration. Stable component IDs, key paths and target locations are required for this scheduling policy.

Read-only checks compare the old and new MSI component identities and verify maintenance/update UI routing. Live wizard, repair, upgrade, rollback and Explorer association behavior remain manual/disposable-VM acceptance; building and ICE validation do not establish those results. See [Packaging.md](Packaging.md).

## Distribution artifacts

Keep only source, assets, guides and concise reviewed validation summaries in Git. Build/install results, raw test reports and intermediary compiler files stay ignored. `Dist/x64/Win/` holds the latest locally built MSI and checksum for upload as release assets; only its guide is tracked. Use a new version for changes after a public release.

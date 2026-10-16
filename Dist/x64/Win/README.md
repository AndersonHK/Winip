# Windows x64 installer

`Winip-0.2.2-x64.msi` is the current installer. It includes
the application, private fonts, Python kernel bridge and applicable license
notices. Python and notebook packages are supplied separately by the user.

Only this guide is tracked in Git. Generated MSI/checksum files are ignored; upload them as release assets when publishing. Build locally if no release has been uploaded.

The installer requires administrator permission and is currently unsigned.
Verify its SHA-256 against `SHA256SUMS.txt` with:

```powershell
Get-FileHash .\Dist\x64\Win\Winip-0.2.2-x64.msi -Algorithm SHA256
```

From the repository root, `python Scripts/package.py` rebuilds the version from
`VERSION`, checks the package, and copies only the completed MSI and checksum
here. Compiler products, payload staging, WiX debug databases and validation
extracts stay in ignored `Out/Packaging/`. Retire the previous MSI from this
directory when publishing a newer version; keep historical releases as release
assets rather than accumulating them in the source tree.

See [the main README](../../../README.md) for usage and
[packaging documentation](../../../Docs/Packaging.md) for build requirements.

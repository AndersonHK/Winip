# Winip 0.3.3 — Windows x86-64

`Winip-0.3.3-x64.msi` is the current 64-bit x86 Windows installer. It includes the
application, private fonts, kernel bridge, guided setup helper and license notices.
The MSI checks Python/kernel readiness automatically and offers skippable setup
with explicit destination confirmation. A working kernel does not require pip.

Only this guide is tracked in Git. The MSI and `SHA256SUMS.txt` are generated release
assets, distributed separately from source. If assets are not available, build
from the repository. The installer needs administrator permission and is unsigned.

Verify the local package from the repository root:

```powershell
Get-FileHash .\Dist\x64\Win\Winip-0.3.3-x64.msi -Algorithm SHA256
Get-Content .\Dist\x64\Win\SHA256SUMS.txt
```

`python Scripts/package.py` builds the version in `VERSION`, verifies the payload,
and replaces the current MSI/checksum only after success. Keep only the current
release in this folder. Build intermediates and useful prior-release comparison
packages belong in ignored `Out/Packaging/`, not alongside the distributable.

See the [main README](../../../README.md), [packaging guide](../../../Docs/Packaging.md)
and [release summary](../../../Docs/Checkpoint-10.md). Linux and macOS packages are
not currently available; future builds must identify their OS and CPU architecture.

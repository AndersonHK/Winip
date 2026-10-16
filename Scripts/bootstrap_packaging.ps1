<# Restore a pinned, repository-local WiX toolchain. Requires .NET SDK and network. #>
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'Out/Packaging'
$env:DOTNET_CLI_HOME = Join-Path $build 'dotnet-home'
$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = '1'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_GENERATE_ASPNET_CERTIFICATE = 'false'
$env:NUGET_PACKAGES = Join-Path $build 'nuget'
$toolsDir = Join-Path $build 'tools'
New-Item -ItemType Directory -Force $toolsDir | Out-Null
if (-not (Test-Path (Join-Path $toolsDir 'wix.exe'))) {
    dotnet tool install wix --version 6.0.2 --tool-path $toolsDir --no-cache
    if ($LASTEXITCODE -ne 0) { throw 'WiX restore failed' }
}
Push-Location $toolsDir
try {
    & (Join-Path $toolsDir 'wix.exe') extension add WixToolset.UI.wixext/6.0.2
    if ($LASTEXITCODE -ne 0) { throw 'WiX UI extension restore failed' }
} finally { Pop-Location }

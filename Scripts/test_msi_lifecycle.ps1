<# Run elevated in a disposable Windows VM. Installs/repairs/uninstalls the MSI twice.
   Refuses to touch a pre-existing Winip installation or registration. #>
param([Parameter(Mandatory=$true)][string]$Msi)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this lifecycle test elevated in a disposable Windows VM.'
}
$msiPath = (Resolve-Path -LiteralPath $Msi).Path
$keys = @('HKLM:\Software\Winip', 'HKLM:\Software\Classes\Winip.Notebook')
$menu = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Winip'
foreach ($key in $keys + @($menu)) {
    if (Test-Path -LiteralPath $key) { throw "Pre-existing Winip state; refusing test: $key" }
}
$registered = Get-ItemProperty -LiteralPath 'HKLM:\Software\RegisteredApplications' -Name Winip -ErrorAction SilentlyContinue
if ($registered) { throw 'Pre-existing Winip RegisteredApplications entry' }
$openWith = 'HKLM:\Software\Classes\.ipynb\OpenWithProgids'
if ((Get-ItemProperty -LiteralPath $openWith -Name 'Winip.Notebook' -ErrorAction SilentlyContinue)) { throw 'Pre-existing Winip Open with entry' }
$runId = [Guid]::NewGuid().ToString('N')
$logs = Join-Path (Split-Path $msiPath) "lifecycle-$runId"
New-Item -ItemType Directory -Path $logs | Out-Null
$install = Join-Path $env:ProgramFiles "Winip Packaging Test $runId"
if (Test-Path -LiteralPath $install) { throw 'Test install folder already exists' }
function Invoke-Msi([string]$Arguments, [string]$Name) {
    $log = Join-Path $logs "$Name.log"
    $process = Start-Process -FilePath "$env:SystemRoot\System32\msiexec.exe" -ArgumentList "$Arguments /qn /norestart /l*v `"$log`"" -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw "MSI returned $($process.ExitCode); inspect $log (3010 means reboot required)." }
}
foreach ($association in @(1,0)) {
    $installed = $false
    try {
        # Finally also attempts removal after a partially failed installation.
        $installed = $true
        Invoke-Msi "/i `"$msiPath`" INSTALLFOLDER=`"$install`" ASSOCIATE_IPYNB=$association" "install-$association"
        foreach ($file in @('Winip.exe','Uninstall.exe','fonts\NotoSans-Regular.ttf','licenses\WiX-LICENSE.txt')) {
            if (-not (Test-Path -LiteralPath (Join-Path $install $file))) { throw "Missing installed file: $file" }
        }
        if ((Test-Path -LiteralPath 'HKLM:\Software\Classes\Winip.Notebook') -ne ($association -eq 1)) { throw 'Association choice was ignored' }
        if (-not (Test-Path -LiteralPath (Join-Path $menu 'Uninstall Winip.lnk'))) { throw 'Missing uninstall shortcut' }
        $font = [IO.Path]::GetFullPath((Join-Path $install 'fonts\NotoSans-Regular.ttf'))
        if (-not $font.StartsWith([IO.Path]::GetFullPath($install) + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe repair-test path' }
        Remove-Item -LiteralPath $font
        Invoke-Msi "/fa `"$msiPath`"" "repair-$association"
        if (-not (Test-Path -LiteralPath $font)) { throw 'Repair did not restore the missing font' }
        if ((Test-Path -LiteralPath 'HKLM:\Software\Classes\Winip.Notebook') -ne ($association -eq 1)) { throw 'Repair changed the association choice' }
        Invoke-Msi "/x `"$msiPath`"" "uninstall-$association"
        $installed = $false
        foreach ($key in $keys + @($menu,$install)) {
            if (Test-Path -LiteralPath $key) { throw "Uninstall left owned state: $key" }
        }
        if ((Get-ItemProperty -LiteralPath $openWith -Name 'Winip.Notebook' -ErrorAction SilentlyContinue)) { throw 'Uninstall left the association' }
        if ((Get-ItemProperty -LiteralPath 'HKLM:\Software\RegisteredApplications' -Name Winip -ErrorAction SilentlyContinue)) { throw 'Uninstall left RegisteredApplications' }
    } finally {
        if ($installed) { Invoke-Msi "/x `"$msiPath`"" "cleanup-$association" }
    }
}
Write-Output "PASS: install, custom folder, association enabled/disabled, repair and native uninstall. Logs: $logs"

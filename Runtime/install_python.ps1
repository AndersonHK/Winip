param([Parameter(Mandatory=$true)][string]$WorkDirectory, [switch]$MetadataOnly, [switch]$VerifyDownloadOnly)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Select-PythonRelease($Releases, [version]$WindowsVersion) {
    # Historical Windows targets need their last supported CPython family.
    $ceiling = if ($WindowsVersion -lt [version]'6.2') { [version]'3.9' }
        elseif ($WindowsVersion -lt [version]'6.3') { [version]'3.9' }
        elseif ($WindowsVersion -lt [version]'10.0') { [version]'3.13' }
        else { [version]'99.0' }
    @($Releases | Where-Object {
        $_.is_published -and -not $_.pre_release -and $_.name -match '^Python (3\.\d+\.\d+)$' -and
        [version]$Matches[1] -lt $ceiling
    } | Sort-Object { [version]($_.name -replace '^Python ', '') } -Descending)
}

function Assert-PythonDownload($File) {
    $uri = [uri]$File.url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'www.python.org' -or
        $uri.AbsolutePath -notmatch '^/ftp/python/\d+\.\d+\.\d+/python-\d+\.\d+\.\d+-amd64\.exe$' -or
        $File.sha256_sum -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'Official metadata did not provide a verifiable stable x64 installer.'
    }
}

function Invoke-PythonInstall {
    $workPath = [IO.Path]::GetFullPath($WorkDirectory)
    if (-not (Test-Path -LiteralPath $workPath -PathType Container)) { throw 'Setup workspace is missing.' }
    $downloadPath = Join-Path $workPath 'python-installer.exe'
    try {
        $releases = Invoke-RestMethod -Uri 'https://www.python.org/api/v2/downloads/release/?is_published=true' -TimeoutSec 30
        $candidates = Select-PythonRelease $releases ([Environment]::OSVersion.Version)
        $selected = $null
        foreach ($release in $candidates) {
            $releaseId = ([uri]$release.resource_uri).Segments[-1].Trim('/')
            if ($releaseId -notmatch '^\d+$') { throw 'Invalid official release identity.' }
            $files = Invoke-RestMethod -Uri ('https://www.python.org/api/v2/downloads/release_file/?release=' + $releaseId) -TimeoutSec 30
            $selected = $files | Where-Object { $_.url -match '/python-\d+\.\d+\.\d+-amd64\.exe$' } | Select-Object -First 1
            if ($selected) { break }
        }
        if (-not $selected) { throw 'No compatible stable Windows x64 installer was found.' }
        Assert-PythonDownload $selected
        Write-Output ('Selected official installer: ' + $selected.url)
        if ($MetadataOnly) { return }
        Invoke-WebRequest -UseBasicParsing -Uri $selected.url -OutFile $downloadPath -TimeoutSec 300
        if ((Get-FileHash -LiteralPath $downloadPath -Algorithm SHA256).Hash -ne $selected.sha256_sum) {
            throw 'Python installer SHA-256 does not match official metadata.'
        }
        $signature = Get-AuthenticodeSignature -LiteralPath $downloadPath
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Python Software Foundation(?:,|$)') {
            throw 'Python installer does not have a valid Python Software Foundation signature.'
        }
        if ($VerifyDownloadOnly) { Write-Output 'Official installer download, SHA-256 and Authenticode verified; no installation performed.'; return }
        # Per-user installation, no PATH/default association/launcher takeover.
        $installer = Start-Process -FilePath $downloadPath -ArgumentList @('/quiet', 'InstallAllUsers=0',
            'Include_pip=1', 'Include_launcher=0', 'AssociateFiles=0', 'PrependPath=0', 'Include_test=0') -WindowStyle Hidden -PassThru -Wait
        if ($installer.ExitCode -notin @(0,3010)) { throw ('Python installer failed with exit code ' + $installer.ExitCode) }
        Write-Output ('Installer completed with exit code ' + $installer.ExitCode + '. Winip must now verify discovery.')
    } finally {
        # Only this invocation's downloaded file is ours. Never remove Python or
        # shared installer caches, even when detection fails after installation.
        if (Test-Path -LiteralPath $downloadPath -PathType Leaf) { Remove-Item -LiteralPath $downloadPath -Force }
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    try { Invoke-PythonInstall; exit 0 }
    catch { Write-Output $_.Exception.Message; exit 1 }
}

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $repo 'Runtime/install_python.ps1') -WorkDirectory (Join-Path $repo 'Artifacts')
$releases = @(
    @{name='Python 3.15.0';is_published=$true;pre_release=$false},
    @{name='Python 3.16.0';is_published=$true;pre_release=$true},
    @{name='Python 3.14.9';is_published=$true;pre_release=$false},
    @{name='Python 3.12.9';is_published=$true;pre_release=$false},
    @{name='Python 3.8.10';is_published=$true;pre_release=$false},
    @{name='Python 3.15.1';is_published=$false;pre_release=$false}
)
$latest = @(Select-PythonRelease $releases ([version]'10.0'))
if ($latest[0].name -ne 'Python 3.15.0') { throw 'Stable release ordering failed.' }
if (@(Select-PythonRelease $releases ([version]'6.1'))[0].name -ne 'Python 3.8.10') { throw 'Windows 7 ceiling failed.' }
if (@(Select-PythonRelease $releases ([version]'6.3'))[0].name -ne 'Python 3.12.9') { throw 'Windows 8.1 ceiling failed.' }
Assert-PythonDownload @{url='https://www.python.org/ftp/python/3.15.0/python-3.15.0-amd64.exe';sha256_sum=('a'*64)}
foreach ($url in @('http://www.python.org/ftp/python/3.15.0/python-3.15.0-amd64.exe',
    'https://evil.invalid/ftp/python/3.15.0/python-3.15.0-amd64.exe',
    'https://www.python.org/ftp/python/3.15.0/python-3.15.0rc1-amd64.exe',
    'https://www.python.org/ftp/python/3.15.0/python-3.15.0-arm64.exe')) {
    $rejected=$false
    try { Assert-PythonDownload @{url=$url;sha256_sum=('a'*64)} } catch { $rejected=$true }
    if (-not $rejected) { throw ('Unsafe installer metadata accepted: '+$url) }
}
Write-Output 'Bootstrap release/platform/URL tests passed. No downloads or installs performed.'

# Simulate the network and installer to exercise cleanup and verification failure
# without installing Python or starting a real installer.
$WorkDirectory = Join-Path $repo ('Artifacts/bootstrap-fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$script:installerCalls = 0
$script:fixtureExitCode = 0
$script:signatureValid = $true
$script:hashValid = $true
function Invoke-RestMethod {
    param($Uri,$TimeoutSec)
    if ($Uri -like '*release_file*') {
        return @{url='https://www.python.org/ftp/python/3.15.0/python-3.15.0-amd64.exe';sha256_sum=('a'*64)}
    }
    return @{name='Python 3.15.0';is_published=$true;pre_release=$false;resource_uri='https://www.python.org/api/v2/downloads/release/123/'}
}
function Invoke-WebRequest {
    param([switch]$UseBasicParsing,$Uri,$OutFile,$TimeoutSec)
    [IO.File]::WriteAllText($OutFile,'fixture, not executable')
}
function Get-FileHash {
    param($LiteralPath,$Algorithm)
    return @{Hash=$(if($script:hashValid){'a'*64}else{'b'*64})}
}
function Get-AuthenticodeSignature {
    param($LiteralPath)
    return @{Status=$(if($script:signatureValid){'Valid'}else{'NotSigned'});SignerCertificate=@{Subject='CN=Python Software Foundation, O=Python Software Foundation, C=US'}}
}
function Start-Process {
    param($FilePath,$ArgumentList,$WindowStyle,[switch]$PassThru,[switch]$Wait)
    if ($WindowStyle -ne 'Hidden' -or $ArgumentList -notcontains 'PrependPath=0' -or $ArgumentList -notcontains 'AssociateFiles=0' -or $ArgumentList -notcontains 'InstallAllUsers=0') { throw 'Installer modified environment ownership defaults.' }
    $script:installerCalls++
    return @{ExitCode=$script:fixtureExitCode}
}
try {
    Invoke-PythonInstall
    if ($script:installerCalls -ne 1 -or (Test-Path (Join-Path $WorkDirectory 'python-installer.exe'))) { throw 'Successful setup did not clean up its download.' }
    $script:fixtureExitCode=1603
    $rejected=$false
    try { Invoke-PythonInstall } catch { $rejected=$true }
    if (-not $rejected -or (Test-Path (Join-Path $WorkDirectory 'python-installer.exe'))) { throw 'Failed installer did not report failure and clean up.' }
    foreach ($mode in @('signature','hash')) {
        $script:signatureValid=$mode -ne 'signature';$script:hashValid=$mode -ne 'hash'
        $rejected=$false
        try { Invoke-PythonInstall } catch { $rejected=$true }
        if (-not $rejected -or $script:installerCalls -ne 2 -or (Test-Path (Join-Path $WorkDirectory 'python-installer.exe'))) { throw 'Unverified payload was launched or not cleaned up.' }
    }
    Write-Output 'Simulated bootstrap success/failure/signature/hash/cleanup tests passed.'
} finally {
    # The unique fixture contains only our small placeholder; never recursive.
    $fixture=Join-Path $WorkDirectory 'python-installer.exe'
    if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture}
    Remove-Item -LiteralPath $WorkDirectory
}

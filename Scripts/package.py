"""Build an isolated Release payload and a native x64 MSI with WiX 6.0.2.

Run bootstrap_packaging.ps1 once to restore the repository-local build tool.
No Python packages, user environment, or target-machine runtime installs needed.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET
from build_support import REPOSITORY_ROOT, child_environment, find_msbuild
from versioning import current_version, product_code

ROOT = REPOSITORY_ROOT
PACKAGE_ROOT = ROOT / 'Out/Packaging'
DISTRIBUTION_ROOT = ROOT / 'Dist/x64/Win'
LICENSE_FILES = ['Winip-LICENSE.txt', 'THIRD-PARTY-NOTICES.md',
                 'md4c-LICENSE.md', 'nlohmann-json-LICENSE.txt',
                 'nlohmann-json-NOTICES.txt', 'WiX-LICENSE.txt', 'WinipUI.wxs']
NS = 'http://wixtoolset.org/schemas/v4/wxs'


def run(args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default=current_version())
    args = parser.parse_args()
    if not re.fullmatch(r'\d{1,3}\.\d{1,3}\.\d{1,5}', args.version):
        parser.error('Use an MSI version such as 0.1.0')
    if any(int(n) > limit for n, limit in zip(args.version.split('.'), (255, 255, 65535))):
        parser.error('MSI version fields must be <= 255.255.65535')
    wix = PACKAGE_ROOT / 'tools/wix.exe'
    extension = PACKAGE_ROOT / 'tools/.wix/extensions/WixToolset.UI.wixext/6.0.2/wixext6/WixToolset.UI.wixext.dll'
    if not wix.exists() or not extension.exists():
        parser.error('Run powershell -File Scripts/bootstrap_packaging.ps1 to restore WiX 6.0.2.')
    environment = child_environment()
    msbuild_executable = find_msbuild(environment)
    payload = PACKAGE_ROOT / 'payload'
    payload.mkdir(parents=True, exist_ok=True)
    version_resource = PACKAGE_ROOT / 'ProductVersion.rc'
    version_text = (ROOT / 'Src/ProductVersion.rc').read_text(encoding='utf-8')
    version_text = re.sub(r'0,\d+,\d+,0', args.version.replace('.', ',')+',0', version_text)
    version_text = re.sub(r'0\.\d+\.\d+', args.version, version_text)
    if not version_resource.exists() or version_resource.read_text(encoding='utf-8') != version_text:
        version_resource.write_text(version_text, encoding='utf-8')
    version_property = '/p:WinipVersionResource=' + str(version_resource)
    run([msbuild_executable, ROOT / 'Winip.vcxproj',
         '/nologo', '/verbosity:minimal', '/p:Configuration=Release', '/p:Platform=x64',
         '/p:TargetName=Winip', '/p:WinipStaticRuntime=true', version_property,
         '/p:OutDir=' + str(payload) + os.sep,
         '/p:IntDir=' + str(PACKAGE_ROOT / 'obj') + os.sep], cwd=ROOT, env=environment)
    run([msbuild_executable, ROOT / 'Installer/Uninstall.vcxproj',
         '/nologo', '/verbosity:minimal', '/p:Configuration=Release', '/p:Platform=x64', version_property], cwd=ROOT, env=environment)
    run([msbuild_executable, ROOT / 'Installer/ShellActions.vcxproj',
         '/nologo', '/verbosity:minimal', '/p:Configuration=Release', '/p:Platform=x64'], cwd=ROOT, env=environment)
    shutil.copy2(ROOT / 'ThirdParty/wix/LICENSE.txt', payload / 'licenses/WiX-LICENSE.txt')
    shutil.copy2(ROOT / 'Installer/WinipUI.wxs', payload / 'licenses/WinipUI.wxs')
    # Enumerate runtime assets only, one stable file/component per target path.
    # PDBs, build products and development executables cannot enter the cabinet.
    ET.register_namespace('', NS)
    element = lambda tag, **attrs: ET.Element('{' + NS + '}' + tag, attrs)
    document = element('Wix')
    fragment = element('Fragment')
    document.append(fragment)
    group = element('ComponentGroup', Id='PayloadFiles')
    fragment.append(group)
    for folder in ('fonts', 'licenses', 'runtime'):
        source = payload / folder
        if not source.is_dir() or not any(source.iterdir()):
            raise RuntimeError('Missing runtime assets: ' + str(source))
        directory_ref = element('DirectoryRef', Id='INSTALLFOLDER')
        fragment.append(directory_ref)
        directory = element('Directory', Id='Winip' + folder.title() + 'Folder', Name=folder)
        directory_ref.append(directory)
        names = ([path.name for path in (ROOT / 'Assets/Fonts').iterdir() if path.is_file()]
                 if folder == 'fonts' else ['kernel_bridge.py', 'install_python.ps1'] if folder == 'runtime' else LICENSE_FILES)
        for path in sorted(source / name for name in names):
            if not path.is_file():
                raise RuntimeError('Unexpected nested payload: ' + str(path))
            identity = 'Asset_' + hashlib.sha256((folder + '/' + path.name).encode()).hexdigest()[:24]
            component = element('Component', Id=identity, Guid='*', Bitness='always64')
            if path.suffix.lower() == '.ttf':
                # Private fonts travel with the app version, not the system font
                # registry. MSI companion files cannot themselves be key paths.
                component.set('Guid', str(uuid.uuid5(uuid.UUID('c99b13c5-f87d-45da-9e97-80900ba0b3da'), 'private-font/' + path.name)))
                component.append(element('File', Id=identity + '_File', Source=str(path), CompanionFile='WinipExe'))
                component.append(element('RegistryValue', Root='HKLM', Key='Software\\Winip\\PrivateFonts',
                                         Name=path.name, Value='1', Type='integer', KeyPath='yes'))
            else:
                component.append(element('File', Id=identity + '_File', Source=str(path), KeyPath='yes'))
            directory.append(component)
            group.append(element('ComponentRef', Id=identity))
    manifest = PACKAGE_ROOT / 'Payload.wxs'
    ET.ElementTree(document).write(manifest, encoding='utf-8', xml_declaration=True)
    output = PACKAGE_ROOT / ('Winip-' + args.version + '-x64.msi')
    run([wix, 'build', '-wx', '-arch', 'x64', '-ext', extension,
         '-d', 'Version=' + args.version, '-d', 'ProductCode='+product_code(args.version), '-d', 'RepoRoot=' + str(ROOT),
         '-d', 'ShellActions=' + str(PACKAGE_ROOT / 'custom-actions/ShellActions.dll'),
         '-d', 'Payload=' + str(payload), '-intermediateFolder', PACKAGE_ROOT / 'wixobj',
         '-o', output, ROOT / 'Installer/Package.wxs', ROOT / 'Installer/WinipUI.wxs', manifest], cwd=ROOT, env=environment)
    # Never replace the distributable with an unchecked build. ICE validation is
    # separate because it needs Windows Installer service access.
    run([sys.executable, ROOT / 'Scripts/packaging_tests.py', output,
         '--version', args.version], cwd=ROOT, env=environment)
    if args.version != current_version():
        print('Upgrade-test fixture:', output)
        return
    DISTRIBUTION_ROOT.mkdir(parents=True, exist_ok=True)
    destination = DISTRIBUTION_ROOT / output.name
    temporary = destination.with_suffix('.msi.tmp')
    shutil.copy2(output, temporary)
    temporary.replace(destination)
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    checksum = DISTRIBUTION_ROOT / 'SHA256SUMS.txt.tmp'
    checksum.write_text(digest + '  ' + destination.name + '\n', encoding='ascii')
    checksum.replace(DISTRIBUTION_ROOT / 'SHA256SUMS.txt')
    print('Installer:', destination)
    print('SHA256:', digest)


if __name__ == '__main__':
    main()

"""Read-only MSI/payload/icon checks and native AppPaths tests; no installation."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import platform
from pathlib import Path
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from build_support import REPOSITORY_ROOT, child_environment, find_msbuild
from versioning import current_version, product_code

ROOT = REPOSITORY_ROOT
PACKAGE_ROOT = ROOT / 'Out/Packaging'
SIZES = {16, 20, 24, 32, 40, 48, 64, 96, 128, 192, 256}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class MsiDatabase:
    def __init__(self, path):
        self.api = C.WinDLL('msi')
        self.api.MsiOpenDatabaseW.argtypes = [W.LPCWSTR, W.LPCWSTR, C.POINTER(W.UINT)]
        self.api.MsiDatabaseOpenViewW.argtypes = [W.UINT, W.LPCWSTR, C.POINTER(W.UINT)]
        self.api.MsiViewExecute.argtypes = [W.UINT, W.UINT]
        self.api.MsiViewFetch.argtypes = [W.UINT, C.POINTER(W.UINT)]
        self.api.MsiRecordGetStringW.argtypes = [W.UINT, W.UINT, W.LPWSTR, C.POINTER(W.DWORD)]
        self.handle = W.UINT()
        require(self.api.MsiOpenDatabaseW(str(path), None, C.byref(self.handle)) == 0, 'Cannot open MSI read-only')

    def rows(self, table, columns):
        view = W.UINT()
        sql = 'SELECT ' + ','.join('`' + col + '`' for col in columns) + ' FROM `' + table + '`'
        require(self.api.MsiDatabaseOpenViewW(self.handle, sql, C.byref(view)) == 0, sql)
        try:
            require(self.api.MsiViewExecute(view, 0) == 0, 'Cannot execute MSI query')
            result = []
            while True:
                record = W.UINT()
                status = self.api.MsiViewFetch(view, C.byref(record))
                if status == 259:
                    break
                require(status == 0, 'Cannot fetch MSI row')
                try:
                    row = []
                    for index in range(1, len(columns) + 1):
                        size = W.DWORD(32768)
                        value = C.create_unicode_buffer(size.value)
                        require(self.api.MsiRecordGetStringW(record, index, value, C.byref(size)) == 0, 'Cannot read MSI field')
                        row.append(value.value)
                    result.append(tuple(row))
                finally:
                    self.api.MsiCloseHandle(record)
            return result
        finally:
            self.api.MsiCloseHandle(view)

    def close(self):
        self.api.MsiCloseHandle(self.handle)


def check_icons(exe):
    icon = (ROOT / 'Src/Winip.ico').read_bytes()
    require(icon == (ROOT / 'Src/small.ico').read_bytes(), 'Small icon differs')
    reserved, kind, count = struct.unpack_from('<HHH', icon)
    require((reserved, kind, count) == (0, 1, len(SIZES)), 'Wrong ICO directory')
    frames = {}
    for i in range(count):
        w, h, _, _, planes, bits, length, offset = struct.unpack_from('<BBBBHHII', icon, 6 + i * 16)
        size = w or 256
        require(size == (h or 256) and planes == 1 and bits == 32, 'Wrong icon format')
        frame = icon[offset:offset + length]
        require(frame[:8] == b'\x89PNG\r\n\x1a\n', 'Expected lossless PNG icon')
        require(struct.unpack_from('>II', frame, 16) == (size, size), 'Wrong PNG resolution')
        require(frame[25] == 6, 'Icon must retain RGBA transparency')
        frames[size] = frame
    require(set(frames) == SIZES, 'Missing icon resolution')
    api = C.WinDLL('kernel32')
    api.LoadLibraryExW.argtypes = [W.LPCWSTR, W.HANDLE, W.DWORD]
    api.LoadLibraryExW.restype = W.HMODULE
    api.FindResourceW.argtypes = [W.HMODULE, C.c_void_p, C.c_void_p]
    api.FindResourceW.restype = W.HANDLE
    api.SizeofResource.argtypes = [W.HMODULE, W.HANDLE]
    api.LoadResource.argtypes = [W.HMODULE, W.HANDLE]
    api.LoadResource.restype = W.HANDLE
    api.LockResource.argtypes = [W.HANDLE]
    api.LockResource.restype = C.c_void_p
    api.FreeLibrary.argtypes = [W.HMODULE]
    module = api.LoadLibraryExW(str(exe), None, 2 | 32)
    require(module, 'Cannot load EXE resources')
    try:
        def resource(identifier, kind):
            found = api.FindResourceW(module, identifier, kind)
            require(found, 'Missing EXE resource')
            return C.string_at(api.LockResource(api.LoadResource(module, found)), api.SizeofResource(module, found))
        for group_id in (107, 108):
            group = resource(group_id, 14)
            require(struct.unpack_from('<H', group, 4)[0] == count, 'Missing embedded icon frame')
            for i in range(count):
                w, h, _, _, planes, bits, length, identifier = struct.unpack_from('<BBBBHHIH', group, 6 + 14 * i)
                require(resource(identifier, 3) == frames[w or 256], 'EXE contains a stale or altered icon')
        require(resource(1, 16), 'Missing version resource')
    finally:
        api.FreeLibrary(module)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('msi', nargs='?', type=Path, default=ROOT / 'Dist/x64/Win' / ('Winip-'+current_version()+'-x64.msi'))
    parser.add_argument('--version', default=current_version(), help='Expected release identity.')
    parser.add_argument('--ice', action='store_true', help='Also run native MSI ICE validation (requires service access).')
    parser.add_argument('--previous-msi', type=Path, help='Read-only component/upgrade compatibility checks against an older release.')
    args = parser.parse_args()
    msi = args.msi.resolve()
    previous = None
    db = MsiDatabase(msi)
    try:
        props = dict(db.rows('Property', ['Property', 'Value']))
        require(props['ProductVersion']==args.version and props['ProductCode']==product_code(args.version), 'Release/product identity mismatch')
        require(props['ALLUSERS'] == '1' and props['ASSOCIATE_IPYNB'] == '1', 'Wrong install/association defaults')
        directories = {row[0]: row[1:] for row in db.rows('Directory', ['Directory', 'Directory_Parent', 'DefaultDir'])}
        require(directories['INSTALLFOLDER'] == ('ProgramFiles64Folder', 'Winip'), 'Wrong install directory')
        components = db.rows('Component', ['Component', 'Attributes', 'KeyPath'])
        require(all(int(row[1]) & 256 for row in components), 'Payload must use x64 components')
        registry = db.rows('Registry', ['Root', 'Key', 'Name', 'Value'])
        require(all(row[0] == '2' for row in registry), 'Unexpected registry hive')
        require(not any('UserChoice' in row[1] for row in registry), 'Must preserve user defaults')
        commands = [r[3] for r in registry if r[1].endswith('shell\\open\\command')]
        require(commands == ['"[INSTALLFOLDER]Winip.exe" "%1"'], 'Shell command must quote both paths')
        require(any(r[1] == 'Software\\Classes\\.ipynb\\OpenWithProgids' and r[2] == 'Winip.Notebook' for r in registry), 'Missing Open with registration')
        require(not any(r[1] == 'Software\\Classes\\.ipynb' and not r[2] for r in registry), 'Must not replace another handler')
        checks = db.rows('CheckBox', ['Property', 'Value'])
        require(('ASSOCIATE_IPYNB', '1') in checks, 'Missing association checkbox')
        conditions = db.rows('Condition', ['Feature_', 'Level', 'Condition'])
        require(('FileAssociation', '2', 'ASSOCIATE_IPYNB <> "1"') in conditions, 'Silent opt-out is missing')
        events = db.rows('ControlEvent', ['Dialog_', 'Control_', 'Event', 'Argument', 'Condition'])
        require(any(r[:4] == ('AssociationsDlg', 'Next', 'AddLocal', 'FileAssociation') for r in events), 'UI opt-in is missing')
        require(any(r[:4] == ('AssociationsDlg', 'Next', 'Remove', 'FileAssociation') for r in events), 'UI opt-out is missing')
        sequence = dict(db.rows('InstallExecuteSequence', ['Action', 'Sequence']))
        for action in ('RemoveRegistryValues', 'RemoveShortcuts', 'RemoveFiles', 'RemoveFolders', 'RemoveExistingProducts'):
            require(action in sequence, 'Native cleanup/upgrade action missing: ' + action)
        require(int(sequence['InstallExecute']) < int(sequence['RemoveExistingProducts']) < int(sequence['InstallFinalize']), 'Update must install files before retiring old product, inside the transaction')
        require('MigrateFeatureStates' in sequence, 'Update must retain installed feature choices')
        require(any(r[:4]==('WelcomeDlg','Next','NewDialog','UpgradeDlg') and 'WIX_UPGRADE_DETECTED' in r[4] for r in events), 'Missing detected-update wizard path')
        require(any(r[:4]==('MaintenanceTypeDlg','RepairButton','NewDialog','VerifyReadyDlg') for r in events), 'Same-version repair path missing')
        if args.previous_msi:
            old = MsiDatabase(args.previous_msi.resolve())
            try:
                old_props = dict(old.rows('Property', ['Property', 'Value']))
                require(old_props['UpgradeCode'] == props['UpgradeCode'], 'Upgrade family changed')
                require(tuple(map(int, old_props['ProductVersion'].split('.'))) < tuple(map(int, props['ProductVersion'].split('.'))), 'Comparison package must be older')
                require(old_props['ProductCode'] != props['ProductCode'], 'New release must have a new product code')
                columns = ['Component', 'ComponentId', 'Directory_', 'Attributes', 'KeyPath']
                old_components = {r[0]: r[1:] for r in old.rows('Component', columns)}
                new_components = {r[0]: r[1:] for r in db.rows('Component', columns)}
                require(all(new_components.get(k) == v for k, v in old_components.items()), 'Existing component identity/location/key path changed; review late upgrade safety')
                previous = {'msi': args.previous_msi.name, 'version': old_props['ProductVersion'], 'preserved_components': len(old_components)}
            finally:
                old.close()
        actions = db.rows('CustomAction', ['Action', 'Type', 'Source', 'Target'])
        require(('RefreshAssociations', '65', 'WinipShellActions', 'RefreshAssociations') in actions, 'Missing shell refresh callback')
        require(int(sequence['RefreshAssociations']) > int(sequence['InstallFinalize']), 'Shell refresh must follow the transaction')
        ui_sequence = {row[0]: (row[1], int(row[2])) for row in db.rows('InstallUISequence', ['Action', 'Condition', 'Sequence'])}
        for action in ('DetectKernelSupport', 'CompleteKernelSetup'):
            require((action, '1', 'WinipShellActions', action) in actions, 'Missing integrated setup action: ' + action)
            require(action not in sequence, 'Dependency setup must not run inside the MSI execute transaction')
            require('NOT Installed' in ui_sequence[action][0] and 'UILevel = 5' in ui_sequence[action][0], 'Setup must be limited to interactive install/update')
        require(ui_sequence['CostFinalize'][1] < ui_sequence['DetectKernelSupport'][1] < ui_sequence['WelcomeDlg'][1], 'Detection must happen automatically before wizard choices')
        require(ui_sequence['CompleteKernelSetup'][1] > ui_sequence['ExecuteAction'][1] and 'WINIP_SETUP_REQUESTED = 1' in ui_sequence['CompleteKernelSetup'][0], 'Guided setup must follow MSI commit and honor skip')
        require('WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT' not in props, 'Obsolete footer checkbox remains')
        require(('WINIP_SETUP_REQUESTED', '1') in checks, 'Missing conditional installation checkbox')
        for dialog in ('AssociationsDlg', 'UpgradeDlg'):
            require(any(r[:4] == (dialog, 'Next', 'NewDialog', 'KernelDependenciesDlg') and 'WINIP_SETUP_STATE <> "ready"' in r[4] for r in events), 'Missing conditional prerequisite route')
            require(any(r[:4] == (dialog, 'Next', 'NewDialog', 'VerifyReadyDlg') and 'WINIP_SETUP_STATE = "ready"' in r[4] for r in events), 'Working kernels must bypass setup pages')
        require(any(r[:4] == ('KernelDependenciesDlg', 'Skip', '[WINIP_SETUP_REQUESTED]', '{}') for r in events), 'Skip must clear installation consent')
        controls = db.rows('Control', ['Dialog_', 'Control', 'Y', 'Height', 'Text'])
        offer = next(row for row in controls if row[:2] == ('KernelDependenciesDlg', 'InstallSupport'))
        require(int(offer[2]) < 150 and int(offer[3]) >= 30, 'Missing-package choice must be prominent in the page body')
        require(all(row[0] in ('RefreshAssociations', 'DetectKernelSupport', 'CompleteKernelSetup') or int(row[1]) in (19, 51) for row in actions), 'Unexpected custom action')
        files = db.rows('File', ['File', 'FileName'])
        file_versions = db.rows('File', ['FileName', 'Version'])
        require(('Winip.exe',current_version()+'.0') in file_versions, 'Executable release version differs from MSI')
        fonts = [row for row in file_versions if row[0].lower().endswith('.ttf')]
        require(len(fonts) == len(list((ROOT / 'Assets/Fonts').glob('*.ttf'))) and all(row[1] == 'WinipExe' for row in fonts), 'Private fonts must follow application companion-file versioning')
        expected = {'WinipExe': PACKAGE_ROOT / 'payload/Winip.exe', 'UninstallExe': PACKAGE_ROOT / 'payload/Uninstall.exe'}
        ns = {'w': 'http://wixtoolset.org/schemas/v4/wxs'}
        for file in ET.parse(PACKAGE_ROOT / 'Payload.wxs').findall('.//w:File', ns):
            expected[file.attrib['Id']] = Path(file.attrib['Source'])
        notices = {
            'Winip-LICENSE.txt': ROOT / 'LICENSE',
            'THIRD-PARTY-NOTICES.md': ROOT / 'THIRD-PARTY-NOTICES.md',
            'md4c-LICENSE.md': ROOT / 'ThirdParty/md4c/LICENSE.md',
            'nlohmann-json-LICENSE.txt': ROOT / 'ThirdParty/nlohmann/LICENSE.MIT',
            'nlohmann-json-NOTICES.txt': ROOT / 'ThirdParty/nlohmann/THIRD-PARTY-NOTICES.txt',
            'WiX-LICENSE.txt': ROOT / 'ThirdParty/wix/LICENSE.txt',
            'WinipUI.wxs': ROOT / 'Installer/WinipUI.wxs',
        }
        payload_names = {path.name for path in expected.values()}
        for name, original in notices.items():
            require(name in payload_names, 'Missing distribution notice: ' + name)
            require(sha(PACKAGE_ROOT / 'payload/licenses' / name) == sha(original), 'Stale notice: ' + name)
        require(sha(PACKAGE_ROOT / 'payload/fonts/LICENSE') == sha(ROOT / 'Assets/Fonts/LICENSE'), 'Stale OFL')
        require({row[0] for row in files} == set(expected), 'Unexpected or missing payload file')
    finally:
        db.close()
    verification = PACKAGE_ROOT / 'verification'
    subprocess.run([str(PACKAGE_ROOT / 'tools/wix.exe'), 'msi', 'decompile', str(msi), '-o', str(verification / 'decompiled.wxs'),
                    '-x', str(verification / 'extracted')], check=True)
    for identifier, source in expected.items():
        require(sha(verification / 'extracted/File' / identifier) == sha(source), 'Cabinet content differs: ' + identifier)
    require(sha(verification / 'extracted/Binary/WinipShellActions') == sha(PACKAGE_ROOT / 'custom-actions/ShellActions.dll'), 'Embedded shell callback differs')
    check_icons(verification / 'extracted/File/WinipExe')
    environment = child_environment()
    msbuild_executable = find_msbuild(environment)
    subprocess.run([str(msbuild_executable), str(ROOT / 'Tests/AppPathsTests.vcxproj'),
                    '/nologo', '/verbosity:minimal', '/p:Platform=x64', '/p:Configuration=Release'], env=environment, check=True)
    with tempfile.TemporaryDirectory(prefix='storage-', dir=str(PACKAGE_ROOT)) as directory:
        require(Path(directory).resolve().parent == PACKAGE_ROOT.resolve(), 'Unsafe temporary test directory')
        subprocess.run([str(PACKAGE_ROOT / 'Tests/AppPathsTests.exe'), directory], check=True)
    ice = None
    if args.ice:
        result = subprocess.run([str(PACKAGE_ROOT / 'tools/wix.exe'), 'msi', 'validate', '-wx', str(msi)],
                                capture_output=True, text=True, check=True)
        ice = {'warnings_as_errors': True, 'exit_code': result.returncode, 'output': result.stdout + result.stderr}
        (verification / 'ice-validation.txt').write_text('Command: wix msi validate -wx ' + msi.name + '\nExit code: 0\n' + ice['output'], encoding='utf-8')
        print('Native MSI ICE: passed with warnings treated as errors.')
    evidence = {
        'msi': msi.name, 'msi_sha256': sha(msi), 'platform': platform.system(),
        'checks': ['MSI properties and optional association UI/silent conditions', 'x64 component ownership',
                   'quoted association command and preservation of user defaults', 'native removal and transactional upgrade actions',
                   'private font companion-file versioning', 'all cabinet file SHA256 hashes', 'embedded shell refresh callback',
                   'both EXE icon groups: 11 RGBA sizes', 'EXE version resource',
                   'native AppPaths Unicode, containment, writable/reused directory and collision tests',
                   'complete license notices and modified MS-RL installer UI source'],
        'icon_sizes': sorted(SIZES),
        'payload_sha256': {key: sha(path) for key, path in expected.items()},
        'live_install_uninstall_tested': False,
        'native_msi_ice': ice,
        'release_version': props['ProductVersion'],
        'product_code': props['ProductCode'],
        'previous_release_compatibility': previous,
    }
    (verification / 'report.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print('PASS: MSI defaults, UI checkbox events, association ownership/quoting, native cleanup/upgrade actions,')
    print('x64 components, cabinet hashes, version resource, 11 embedded RGBA icon sizes, and AppPaths.')
    print('MSI SHA256:', sha(msi))


if __name__ == '__main__':
    main()

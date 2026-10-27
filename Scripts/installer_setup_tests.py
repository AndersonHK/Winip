"""Read-only MSI prerequisite detection and conditional page-routing checks.

Uses a restricted MSI session, never installs Winip, opens dialogs, or modifies
Python packages. The native detection entry point only probes and sets properties.
"""
import ctypes as C
from ctypes import wintypes as W
import json
from pathlib import Path
from packaging_tests import MsiDatabase, require
from versioning import current_version

ROOT = Path(__file__).resolve().parents[1]


def main():
    msi = ROOT / 'Out/Packaging' / ('Winip-' + current_version() + '-x64.msi')
    api = C.WinDLL('msi')
    ole = C.OleDLL('ole32')
    ole.CoInitialize(None)
    api.MsiOpenPackageExW.argtypes = [W.LPCWSTR, W.DWORD, C.POINTER(W.UINT)]
    api.MsiGetPropertyW.argtypes = [W.UINT, W.LPCWSTR, W.LPWSTR, C.POINTER(W.DWORD)]
    api.MsiSetPropertyW.argtypes = [W.UINT, W.LPCWSTR, W.LPCWSTR]
    api.MsiEvaluateConditionW.argtypes = [W.UINT, W.LPCWSTR]
    api.MsiSetInternalUI.argtypes = [W.UINT, C.c_void_p]
    previous_ui = api.MsiSetInternalUI(2, None)
    session = W.UINT()
    require(api.MsiOpenPackageExW(str(msi), 1, C.byref(session)) == 0, 'Cannot open restricted MSI session')
    database = MsiDatabase(msi)
    try:
        action = C.WinDLL(str(ROOT / 'Out/Packaging/custom-actions/ShellActions.dll')).DetectKernelSupport
        action.argtypes = [W.UINT]
        action.restype = W.UINT
        require(action(session) == 0, 'Native automatic detection failed')

        def get(name):
            size = W.DWORD(32768)
            value = C.create_unicode_buffer(size.value)
            require(api.MsiGetPropertyW(session, name, value, C.byref(size)) == 0, 'Missing setup property')
            return value.value

        detected = {name: get(name) for name in ('WINIP_SETUP_STATE', 'WINIP_SETUP_TITLE', 'WINIP_SETUP_CHOICE', 'WINIP_SETUP_REQUESTED')}
        require(detected['WINIP_SETUP_STATE'] in ('ready', 'python', 'pip', 'kernel', 'repair'), 'Unknown detection state')
        require(detected['WINIP_SETUP_REQUESTED'] == ('' if detected['WINIP_SETUP_STATE'] == 'ready' else '1'), 'Guidance must default on only for incomplete environments')
        events = database.rows('ControlEvent', ['Dialog_', 'Control_', 'Event', 'Argument', 'Condition'])
        matrix = []
        for state in ('ready', 'python', 'pip', 'kernel', 'repair'):
            api.MsiSetPropertyW(session, 'WINIP_SETUP_STATE', state)
            for dialog in ('AssociationsDlg', 'UpgradeDlg'):
                destinations = [row[3] for row in events if row[:3] == (dialog, 'Next', 'NewDialog') and api.MsiEvaluateConditionW(session, row[4]) == 1]
                expected = 'VerifyReadyDlg' if state == 'ready' else 'KernelDependenciesDlg'
                require(destinations == [expected], 'Wrong or ambiguous setup route: ' + repr((state, dialog, destinations)))
                matrix.append({'state': state, 'from': dialog, 'to': expected})
        sequences = {row[0]: row[1] for row in database.rows('InstallUISequence', ['Action', 'Condition'])}
        api.MsiSetPropertyW(session, 'Installed', '')
        api.MsiSetPropertyW(session, 'UILevel', '5')
        api.MsiSetPropertyW(session, 'WINIP_SETUP_REQUESTED', '1')
        require(api.MsiEvaluateConditionW(session, sequences['CompleteKernelSetup']) == 1, 'Default continuation did not run')
        api.MsiSetPropertyW(session, 'WINIP_SETUP_REQUESTED', '')
        require(api.MsiEvaluateConditionW(session, sequences['CompleteKernelSetup']) == 0, 'Skip did not suppress continuation')
        api.MsiSetPropertyW(session, 'WINIP_SETUP_REQUESTED', '1')
        api.MsiSetPropertyW(session, 'UILevel', '2')
        require(api.MsiEvaluateConditionW(session, sequences['CompleteKernelSetup']) == 0, 'Silent MSI must not open setup')
        folder = ROOT / 'Artifacts/kernel-setup-tests'
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'installer-routing.json').write_text(json.dumps({'detected': detected, 'routes': matrix, 'skip_and_silent_checks': 'passed'}, indent=2), encoding='utf-8')
        print('Automatic native detection, default guidance, ten conditional routes, skip and silent-install checks passed.')
    finally:
        database.close()
        api.MsiCloseHandle(session)
        api.MsiSetInternalUI(previous_ui, None)
        ole.CoUninitialize()


if __name__ == '__main__':
    main()

"""Headless readiness tests with a disposable, package-free virtual environment.

No host packages are installed, upgraded or removed. Stub modules exercise only
readiness classification; they do not certify a functioning Jupyter kernel.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / 'Out/Tests/x64/Release/WinipTests.exe'


def main():
    subprocess.run([str(TESTS), '--setup-tests', str(ROOT), sys.executable], check=True)
    results = []
    with tempfile.TemporaryDirectory(prefix='setup-fixture-', dir=ROOT / 'Artifacts') as directory:
        environment = Path(directory)
        subprocess.run([sys.executable, '-m', 'venv', '--without-pip', str(environment)], check=True)
        python = environment / 'Scripts/python.exe'
        site = environment / 'Lib/site-packages'

        def inspect(label):
            value = json.loads(subprocess.check_output([str(TESTS), '--python-readiness', str(python)], encoding='utf-8'))
            results.append({'case': label, **value})
            return value

        missing = inspect('no pip or kernel packages')
        assert not missing['ready'] and not missing['pip'] and missing['virtual'], missing
        assert set(missing['missing']) == {'ipython', 'ipykernel', 'jupyter_client'}, missing
        for module in ('IPython', 'ipykernel', 'jupyter_client'):
            (site / (module + '.py')).write_text('# readiness fixture only\n', encoding='utf-8')
        ready = inspect('importable kernel modules without pip')
        assert ready['ready'] and not ready['pip'], ready
        (site / 'IPython.py').unlink()
        incomplete = inspect('IPython missing from otherwise present stack')
        assert incomplete['missing'] == ['ipython'] and not incomplete['ready'], incomplete
        (site / 'ipykernel.py').write_text('import IPython\n', encoding='utf-8')
        cascading = inspect('missing IPython also breaks installed ipykernel')
        assert cascading['missing'] == ['ipython'] and cascading['package_error'] and cascading['setup_stage'] == 'pip', cascading
        (site / 'IPython.py').write_text("raise ImportError('fixture broken dependency')\n", encoding='utf-8')
        broken = inspect('installed module import failure')
        assert not broken['ready'] and not broken['missing'] and 'fixture broken dependency' in broken['package_error'] and broken['setup_stage'] == 'repair', broken
        invalid = json.loads(subprocess.check_output([str(TESTS), '--python-readiness', str(environment / 'missing.exe')], encoding='utf-8'))
        assert invalid['error'] and not invalid['ready'] and not invalid['executable'], invalid
        results.append({'case': 'invalid explicit interpreter', **invalid})
    (ROOT / 'Artifacts/kernel-setup-tests/readiness.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print('Readiness fixture matrix passed; no host package changes.')


if __name__ == '__main__':
    main()

"""Exercise the application's own missing/malformed-file modal and preserved document checks."""
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
name = sys.argv[1] if len(sys.argv) > 1 else 'Winip-dev-18'
directory = root / 'Artifacts' / ('open-error-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
directory.mkdir()
shutil.copyfile(root / 'Tests/Fixtures/visualization.ipynb', directory / 'original.ipynb')
(directory / 'malformed-notebook.ipynb').write_text('{"nbformat":4,"cells":[broken JSON')
(directory / 'invalid-utf8.ipynb').write_bytes(b'{"nbformat":4,"cells":[],"metadata":{"broken":"\xff"}}')
subprocess.run([str(root / 'Out/Bin/x64/Release' / (name + '.exe')), str(directory / 'original.ipynb'),
                '--open-error-probe', str(directory), '--no-snapshots'], cwd=root,
               env={k.upper(): v for k, v in os.environ.items()}, timeout=35, check=True)
metrics = json.loads((directory / 'metrics.json').read_text(encoding='utf-8'))
assert not metrics['renderer_failed'] and not any('failure' in step for step in metrics['probe']), metrics['probe']
assert any(step.get('missing_file_modal') and step.get('malformed_file_modal') and step.get('invalid_utf8_modal') and step.get('previous_document_preserved') for step in metrics['probe'])
print(json.dumps({'artifact': str(directory), 'checks': metrics['probe']}, indent=2))

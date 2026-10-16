"""Continuous app-owned resize workload, without snapshots or external automation."""
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime

root = Path(__file__).resolve().parents[1]
name = sys.argv[1]
fixture = sys.argv[2] if len(sys.argv) > 2 else 'visualization.ipynb'
directory = root / 'Artifacts' / ('resize-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
directory.mkdir(parents=True)
subprocess.run([str(root / 'Out/Bin/x64/Release' / (name + '.exe')),
                str(root / 'Tests/Fixtures' / fixture), '--resize-probe', str(directory), '--no-snapshots'],
               cwd=root, env={key.upper(): value for key, value in os.environ.items()}, timeout=45, check=True)
metrics = json.loads((directory / 'metrics.json').read_text(encoding='utf-8'))
assert not metrics['renderer_failed']
assert not any('failure' in step for step in metrics['probe'])
work = next(step for step in metrics['probe'] if 'resize_steps' in step)
assert work['resize_steps'] == 320 and work['frames'] >= 312
for frame in metrics['frame_statistics']:
    assert frame['width'] == frame['allocation_width'] and frame['height'] == frame['allocation_height']
    assert not frame['snapshot']
print(json.dumps({'workload': work, 'stages': metrics['render_stages'], 'artifact': str(directory)}, indent=2))

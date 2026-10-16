"""App-owned real-kernel acceptance on a disposable notebook copy."""
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
name = sys.argv[1] if len(sys.argv) > 1 else 'Winip-dev-11'
directory = root/'Artifacts'/('kernel-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
directory.mkdir(parents=True)
notebook = directory/'Hello World.ipynb'
shutil.copyfile(root/'Tests/Fixtures/hello-world.ipynb', notebook)
subprocess.run([str(root/'Out/Bin/x64/Release'/(name+'.exe')), str(notebook), '--kernel-probe', str(directory)] +
               (['--no-snapshots'] if '--no-snapshots' in sys.argv else []), cwd=root,
               env={key.upper(): value for key,value in os.environ.items()}, timeout=65, check=True)
metrics = json.loads((directory/'metrics.json').read_text(encoding='utf-8'))
assert not metrics['renderer_failed']
assert not any('failure' in step for step in metrics['probe']), metrics['probe']
assert any(step.get('hello_world') and step.get('save_reopen') for step in metrics['probe'])
saved = json.loads(notebook.read_text(encoding='utf-8'))
assert saved['cells'][1]['outputs'][0]['text'] == 'Hello, world!\n'
assert saved['cells'][2]['outputs'][0]['text'] == '42\n'
idle = [step for step in metrics['probe'] if 'idle_frames' in step]
assert len(idle)==3 and all(step['idle_frames']==0 and step['idle_cpu_ms']<=10 and step['kernel_idle_cpu_ms']<=10 for step in idle), idle
assert metrics['ui_over_6ms']==0, metrics['ui_max_ms']
from PIL import Image
for bitmap in directory.glob('*.bmp'):
    Image.open(bitmap).save(bitmap.with_suffix('.png'))
print(json.dumps({'artifact':str(directory),'ui_max_ms':metrics['ui_max_ms'],'probe':metrics['probe']},indent=2))

"""App-owned editing acceptance on a disposable copy; never executes notebook code."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime

root = Path(__file__).resolve().parents[1]
name = sys.argv[1] if len(sys.argv) > 1 else 'Winip-dev-09'
directory = root / 'Artifacts' / ('editing-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
directory.mkdir(parents=True)
source = directory / 'Editing notebook.ipynb'
shutil.copyfile(root / 'Tests/Fixtures/visualization.ipynb', source)
original = json.loads(source.read_text(encoding='utf-8'))
subprocess.run([str(root / 'Out/Bin/x64/Release' / (name + '.exe')), str(source), '--editing-probe', str(directory)] +
               (['--no-snapshots'] if '--no-snapshots' in sys.argv else []), cwd=root,
               env={key.upper(): value for key, value in os.environ.items()}, timeout=45, check=True)
metrics = json.loads((directory / 'metrics.json').read_text(encoding='utf-8'))
assert not metrics['renderer_failed']
assert not any('failure' in step for step in metrics['probe']), metrics['probe']
assert any(step.get('save_reopen') for step in metrics['probe'])
assert any(step.get('markdown_click_away') and step.get('modal_shortcuts') for step in metrics['probe'])
saved = json.loads(source.read_text(encoding='utf-8'))
assert len(saved['cells']) == len(original['cells']) + 1
assert saved['cells'][0]['source'] == '# Edited Markdown\n\nText with Unicode: Ελληνικά and **bold**, *italic*, `code`.\n\n- First item\n- Second item'
assert saved['cells'][1]['cell_type'] == 'code'
assert saved['cells'][1]['source'] == 'answer = 42\nprint(answer)'
assert saved['cells'][2]['source'] == "value = '日本語 😀'\nprint(value)"
expected = original.copy()
expected['cells'] = [cell.copy() for cell in original['cells']]
expected['cells'][0]['source'] = saved['cells'][0]['source']
expected['cells'][1]['source'] = saved['cells'][2]['source']
expected['cells'].insert(1, saved['cells'][1])
assert saved == expected, 'Unrelated notebook content changed'
idle = [step for step in metrics['probe'] if 'idle_frames' in step]
assert len(idle) == 3 and all(step['idle_frames'] == 0 and step['idle_cpu_ms'] <= 10 for step in idle), idle
assert metrics['ui_over_6ms'] == 0, metrics['ui_max_ms']
from PIL import Image
for bitmap in directory.glob('*.bmp'):
    Image.open(bitmap).save(bitmap.with_suffix('.png'))
print(json.dumps({'artifact': str(directory), 'ui_max_ms': metrics['ui_max_ms'], 'probe': metrics['probe']}, indent=2))

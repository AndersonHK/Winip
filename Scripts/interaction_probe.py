"""App-owned input/checkpoint probe on a disposable notebook copy."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime

root = Path(__file__).resolve().parents[1]
configuration = sys.argv[1] if len(sys.argv) > 1 else 'Release'
name = sys.argv[sys.argv.index('--name')+1] if '--name' in sys.argv else ('Winip-next' if '--next' in sys.argv else 'Winip')
directory = root / 'Artifacts' / ('interaction-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
directory.mkdir(parents=True)
source = directory / 'Original notebook.ipynb'
shutil.copyfile(root / 'Tests/Fixtures/visualization.ipynb', source)
original_hash = hashlib.sha256(source.read_bytes()).hexdigest()
subprocess.run([str(root / 'Out/Bin/x64' / configuration / (name + '.exe')), str(source), '--interaction-probe', str(directory)] + (['--no-snapshots'] if '--no-snapshots' in sys.argv else []),
               cwd=root, env={key.upper(): value for key, value in os.environ.items()}, timeout=35, check=True)
metrics = json.loads((directory / 'metrics.json').read_text(encoding='utf-8'))
print(json.dumps(metrics, indent=2))
assert not metrics['renderer_failed']
assert not any('failure' in step for step in metrics['probe'])
rename_step = next(step for step in metrics['probe'] if step.get('rename_published'))
menu_step = next(step for step in metrics['probe'] if 'menu_keyboard_rename' in step)
assert menu_step['menu_keyboard_rename']
assert menu_step['native_scrollbar_absent']
renamed = Path(rename_step['renamed_path'])
assert renamed.parent == directory
assert not source.exists()
assert hashlib.sha256(renamed.read_bytes()).hexdigest() == original_hash
assert metrics['python_bits'] == 64 and not metrics['python_error']
assert metrics['ui_over_6ms'] == 0
from PIL import Image
if '--no-snapshots' not in sys.argv:
    full = Image.open(directory / 'fullscreen.bmp')
    geometry = next(step for step in metrics['probe'] if 'outer_width' in step)
    assert full.size == (geometry['outer_width'], geometry['outer_height'])
    # Expanded edges must be populated, not black/uninitialized back-buffer regions.
    assert min(full.convert('RGB').getpixel((full.width-4, full.height//2))) > 180
    assert min(full.convert('RGB').getpixel((full.width//2, full.height-4))) > 180
for bitmap in directory.glob('*.bmp'):
    frame = Image.open(bitmap)
    if bitmap.name.startswith('resize-cycle-'):
        index = int(bitmap.stem.rsplit('-', 1)[1])
        assert frame.size == ((1280 if index % 2 == 0 else 2400), 1200)
        for x, y in [(frame.width-4, frame.height//2), (4, frame.height//2), (frame.width//2, frame.height-4)]:
            assert min(frame.convert('RGB').getpixel((x,y))) > 180
    frame.save(bitmap.with_suffix('.png'))
frames = metrics['frame_statistics']
for frame in frames:
    assert frame['allocation_width'] == frame['width']
    assert frame['allocation_height'] == frame['height']
print('Window/title/Python checkpoint passed:', directory)

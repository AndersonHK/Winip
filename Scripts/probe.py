"""Exercise the app's own integration probe. No external desktop automation."""
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime

root = Path(__file__).resolve().parents[1]
configuration = sys.argv[1] if len(sys.argv) > 1 else 'Release'
name = sys.argv[sys.argv.index('--name')+1] if '--name' in sys.argv else ('Winip-next' if '--next' in sys.argv else 'Winip')
force_sdr = '--sdr' in sys.argv
long_lines = '--long-lines' in sys.argv
directory = root / 'Artifacts' / ('probe-' + configuration + ('-SDR' if force_sdr else '') + ('-long-lines' if long_lines else ''))
directory.mkdir(parents=True, exist_ok=True)
env = {key.upper(): value for key, value in os.environ.items()}
startup = subprocess.STARTUPINFO()
startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 1
subprocess.run([str(root / 'Out/Bin/x64' / configuration / (name + '.exe')),
                str(root / 'Tests/Fixtures' / ('long-lines.ipynb' if long_lines else 'visualization.ipynb')), '--probe', str(directory)] + (['--sdr'] if force_sdr else []) + (['--no-snapshots'] if '--no-snapshots' in sys.argv else []),
               cwd=root, env=env, startupinfo=startup, timeout=45, check=True)
metrics = json.loads((directory / 'metrics.json').read_text(encoding='utf-8'))
(directory / ('run-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.json')).write_text(json.dumps(metrics,indent=2))
print(json.dumps(metrics, indent=2))
assert not metrics['renderer_failed']
if force_sdr:
    assert not metrics['hdr']
assert not any('failure' in step for step in metrics['probe'])
assert metrics['probe'][0]['thick_frame']
assert metrics['probe'][0]['native_menu_absent']
assert metrics['probe'][0]['right_edge_hit'] == 11  # HTRIGHT
layouts = [step for step in metrics['probe'] if 'phase' in step]
assert len(layouts) == 4
for step, logical_width, scale in zip(layouts, [420, 640, 960, 1200], [1, 1.5, 2, 1]):
    assert step['width_px'] == logical_width * scale
    assert step['height_px'] == 600 * scale
assert any(step.get('scroll_end', 0) > 0 for step in metrics['probe'])
horizontal = next(step for step in metrics['probe'] if 'horizontal_end' in step)
assert horizontal['horizontal_end'] == max(0, horizontal['content_width'] - horizontal['horizontal_page'])
if long_lines:
    assert horizontal['horizontal_end'] > 1000
    assert any(step.get("scrollbar_drag") for step in metrics["probe"])
assert metrics['probe'][-1].get('restored'), 'Probe closed before completing the quiet-idle/restore sequence'
idle_samples = [step for step in metrics['probe'] if 'idle_seconds' in step]
assert idle_samples[-1]['quiet_streak'] == 3
for idle in idle_samples:
    if idle['input_active']:
        continue  # Retained in the report; an input-active window is not idle.
    assert idle['idle_presented_frames'] == 0
    assert idle['idle_cpu_ms'] <= 10
assert metrics['ui_over_6ms'] == 0
try:
    from PIL import Image
    for bitmap in directory.glob('*.bmp'):
        Image.open(bitmap).save(bitmap.with_suffix('.png'))
except ImportError:
    print('Pillow unavailable; GPU snapshots remain as BMP files.')
print('GPU/window integration probe passed')

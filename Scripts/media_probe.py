"""App-owned media interaction/fidelity probe; never executes notebook cells."""
from datetime import datetime
import json, os, subprocess, sys
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
name=sys.argv[1] if len(sys.argv)>1 else 'Winip-dev-24'
notebook=Path(sys.argv[2]).resolve() if len(sys.argv)>2 and not sys.argv[2].startswith('--') else root/'Tests/Fixtures/markdown-images.ipynb'
directory=root/'Artifacts'/('media-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
directory.mkdir(parents=True)
subprocess.run([str(root/'Out/Bin/x64/Release'/(name+'.exe')),str(notebook),'--media-probe',str(directory)]+(['--no-snapshots'] if '--no-snapshots' in sys.argv else []),cwd=root,env={k.upper():v for k,v in os.environ.items()},timeout=100,check=True)
metrics=json.loads((directory/'metrics.json').read_text(encoding='utf-8'))
assert not metrics['renderer_failed'] and not any('failure' in step for step in metrics['probe']),metrics['probe']
assert any(step.get('fit_full_fit') and step.get('no_execution') and step.get('uploaded_images')==step.get('eager_images') for step in metrics['probe'])
idle=[step for step in metrics['probe'] if 'idle_frames' in step]
assert len(idle)==3 and all(step['idle_frames']==0 and step['idle_cpu_ms']<=10 for step in idle),idle
assert metrics['ui_over_6ms']==0,metrics['ui_max_ms']
for frame in metrics['frame_statistics']:
 assert frame['width']==frame['allocation_width'] and frame['height']==frame['allocation_height']
for path in directory.glob('*.bmp'):Image.open(path).save(path.with_suffix('.png'))
print(json.dumps({'artifact':str(directory),'ui_max_ms':metrics['ui_max_ms'],'probe':metrics['probe']},indent=2))

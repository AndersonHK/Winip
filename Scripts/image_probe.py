"""Real Pillow output, fresh counters and HDR settings through app-owned handlers."""
from datetime import datetime
import json, os, subprocess, sys
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
name=sys.argv[1] if len(sys.argv)>1 else 'Winip-dev-16'
directory=root/'Artifacts'/('image-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
directory.mkdir(parents=True)
notebook=directory/'Images.ipynb'
fixture=json.loads((root/'Tests/Fixtures/images.ipynb').read_text(encoding='utf-8'))
fixture['cells'][0]['source']='import time; time.sleep(1)\n'+fixture['cells'][0]['source']
notebook.write_text(json.dumps(fixture),encoding='utf-8')
subprocess.run([str(root/'Out/Bin/x64/Release'/(name+'.exe')),str(notebook),'--image-probe',str(directory)]+
    (['--no-snapshots'] if '--no-snapshots' in sys.argv else []),cwd=root,
    env={k.upper():v for k,v in os.environ.items()},timeout=65,check=True)
metrics=json.loads((directory/'metrics.json').read_text(encoding='utf-8'))
assert not metrics['renderer_failed'] and not any('failure' in step for step in metrics['probe']),metrics['probe']
assert any(step.get('live_image') and step.get('reopen_resets_kernel') for step in metrics['probe'])
idle=[step for step in metrics['probe'] if 'idle_frames' in step]
assert len(idle)==3 and all(step['idle_frames']==0 and step['idle_cpu_ms']<=10 for step in idle),idle
assert metrics['ui_over_6ms']==0,metrics['ui_max_ms']
for p in directory.glob('*.bmp'):Image.open(p).save(p.with_suffix('.png'))
print(json.dumps({'artifact':str(directory),'ui_max_ms':metrics['ui_max_ms'],'probe':metrics['probe']},indent=2))

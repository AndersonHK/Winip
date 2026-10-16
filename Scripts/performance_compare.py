"""Sequential baseline/candidate app-owned probes; no builds or screenshots during timing."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('baseline')
parser.add_argument('candidate')
parser.add_argument('--group', choices=['resize', 'interaction'], default='resize')
args = parser.parse_args()
directory = root / 'Artifacts' / ('comparison-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
directory.mkdir()
results = []
if args.group == 'resize':
    scenarios = [('resize', fixture) for fixture in ['visualization.ipynb', 'long-lines.ipynb', 'images.ipynb', 'large.ipynb']]
    repeats = 3
else:
    scenarios = [(kind, None) for kind in ['editing', 'navigator', 'image', 'kernel', 'interaction']]
    repeats = 1
for repeat in range(repeats):
    for kind, fixture in scenarios:
        for name in ([args.baseline, args.candidate] if repeat % 2 == 0 else [args.candidate, args.baseline]):
            command = [sys.executable, str(root / 'Scripts' / (kind + '_probe.py'))]
            if kind == 'interaction':
                command += ['Release', '--name', name, '--no-snapshots']
            else:
                command += [name] + ([fixture] if fixture else ['--no-snapshots'])
            label = f'{repeat}-{kind}-{fixture or "default"}-{name}'
            before = set((root / 'Artifacts').glob(kind + '-*'))
            with (directory / (label + '.log')).open('w', encoding='utf-8') as log:
                run = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT)
            folders = [p for p in set((root / 'Artifacts').glob(kind + '-*')) - before if p.is_dir()]
            if len(folders) != 1:
                raise RuntimeError('Cannot identify probe evidence: ' + label)
            metric = json.loads((folders[0] / 'metrics.json').read_text(encoding='utf-8'))
            work = next((step for step in metric['probe'] if 'resize_steps' in step), {})
            row = {'scenario': kind + '/' + (fixture or 'default'), 'name': name, 'repeat': repeat,
                   'passed': run.returncode == 0, 'artifact': str(folders[0]), 'ui_max_ms': metric['ui_max_ms'],
                   'window_transition_max_ms': metric['window_transition_max_ms'], 'workload': work,
                   'checks': metric['probe']}
            results.append(row)
            (directory / 'runs.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
            print(label, 'PASS' if row['passed'] else 'FAIL', flush=True)
comparisons = []
for scenario in sorted({row['scenario'] for row in results}):
    item = {'scenario': scenario}
    for name in [args.baseline, args.candidate]:
        rows = [row for row in results if row['name'] == name and row['scenario'] == scenario]
        item[name] = {'all_passed': all(row['passed'] for row in rows),
                      'max_ui_ms': max(row['ui_max_ms'] for row in rows)}
        if args.group == 'resize':
            item[name].update({key: statistics.median(row['workload'][key] for row in rows)
                               for key in ['process_cpu_ms', 'wall_ms', 'frames']})
    comparisons.append(item)
report = {'artifacts_sha256': {name: hashlib.sha256((root / 'Out/Bin/x64/Release' / (name + '.exe')).read_bytes()).hexdigest()
                              for name in [args.baseline, args.candidate]}, 'comparisons': comparisons, 'runs': results}
(directory / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'artifact': str(directory), 'comparisons': comparisons}, indent=2))
sys.exit(0 if all(row['passed'] for row in results) else 1)

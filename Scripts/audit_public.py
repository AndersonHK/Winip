"""Inventory public candidates and flag likely private data; read-only, stdlib only.

Run before publication. This heuristic check does not certify absence of PII.
Review binary/media contents, licensing, Git history and commit identities too.
The report contains paths/categories and hashes, never matching secret values.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RULES = {
    'user-profile-path': re.compile(r'(?i:\b[A-Z]:[\\/]+Users[\\/]+[^\\/\s<>"\']+)|(?<![\w/])/(?:home|Users)/[^/\s<>]+'),
    'credential': re.compile(r'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16}|sk-[A-Za-z0-9]{32,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
    'machine-identity': re.compile(r'S-1-5-21-[0-9-]+|\bDESKTOP-[A-Z0-9]{5,}\b'),
}
PRIVATE_EXTENSIONS = {'.suo', '.user', '.db', '.ipch', '.pch', '.pdb', '.wixpdb',
                      '.obj', '.tlog', '.pyc', '.exe', '.dll', '.lib', '.exp',
                      '.ilk', '.iobj', '.ipdb', '.msi'}


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def findings(data):
    # Inspect UTF-8/ASCII and both UTF-16 alignments, including binary strings.
    texts = [data.decode('utf-8', errors='replace')]
    texts.extend(data[offset:].decode('utf-16-le', errors='replace') for offset in (0, 1))
    hits = {name for name, pattern in RULES.items() if any(pattern.search(text) for text in texts)}
    for key in ('USERNAME', 'COMPUTERNAME'):
        value = os.environ.get(key, '')
        if len(value) >= 4:
            pattern = re.compile(r'(?i)(?<![\w])' + re.escape(value) + r'(?![\w])')
            if any(pattern.search(text) for text in texts):
                hits.add('current-' + key.lower())
    return sorted(hits)


def inventory():
    tracked = set(git('ls-files', '-z').decode('utf-8').split('\0')) - {''}
    candidates = tracked | (set(git('ls-files', '--others', '--exclude-standard', '-z').decode('utf-8').split('\0')) - {''})
    rows = []
    seen = set()
    for name in sorted(candidates):
        path = ROOT / name
        if not path.is_file():
            rows.append({'path': name, 'status': 'removed'})
            continue
        actual_name = path.resolve().relative_to(ROOT).as_posix()
        if actual_name in seen:
            continue
        seen.add(actual_name)
        data = path.read_bytes()
        flags = findings(data)
        if name.split('/')[0].lower() in ('.vs', 'out', 'artifacts') or path.suffix.lower() in PRIVATE_EXTENSIONS:
            flags.append('generated-or-private-file')
        # Resources can be UTF-16 source rather than binary assets.
        kind = 'utf16-text' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'binary' if b'\0' in data[:8192] else 'text'
        rows.append({'path': actual_name, 'status': 'tracked' if name in tracked else 'new',
                     'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                     'kind': kind, 'findings': flags})
    return rows


def history_inventory():
    objects = git('rev-list', '--objects', '--all').decode('utf-8').splitlines()
    process = subprocess.Popen(['git', 'cat-file', '--batch'], cwd=ROOT,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    rows = []
    counts = {'blobs_scanned': 0, 'commits_scanned': 0, 'commits_with_identity_metadata': 0}
    try:
        for entry in objects:
            oid, _, name = entry.partition(' ')
            process.stdin.write((oid + '\n').encode('ascii'))
            process.stdin.flush()
            header = process.stdout.readline().decode('ascii').split()
            size = int(header[2])
            data = process.stdout.read(size)
            if len(data) != size or process.stdout.read(1) != b'\n':
                raise RuntimeError('Incomplete Git object read')
            if header[1] not in ('blob', 'commit'):
                continue
            counts['blobs_scanned' if header[1] == 'blob' else 'commits_scanned'] += 1
            if header[1] == 'commit' and re.search(rb'(?m)^(author|committer) .+ <.+>', data):
                counts['commits_with_identity_metadata'] += 1
            flags = findings(data)
            if name.startswith('.vs/') or Path(name).suffix.lower() in PRIVATE_EXTENSIONS:
                flags.append('generated-or-private-file')
            if flags:
                rows.append({'object': oid, 'path': name, 'kind': header[1], 'findings': flags})
    finally:
        process.stdin.close()
        process.stdout.close()
        process.wait()
    return {**counts, 'flagged_objects': rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--history', action='store_true', help='Also scan all reachable Git blobs and commit messages.')
    parser.add_argument('--report', type=Path, default=ROOT / 'Out/publication/audit.json')
    args = parser.parse_args()
    rows = inventory()
    flagged = [row for row in rows if row.get('findings')]
    result = {'scope': 'tracked working files plus non-ignored new files; deleted files recorded',
              'files': rows, 'flagged_files': len(flagged)}
    if args.history:
        result['history'] = history_inventory()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Files inventoried:', len(rows), '| flagged working files:', len(flagged))
    for row in flagged:
        print(row['path'] + ': ' + ', '.join(row['findings']))
    if args.history:
        history = result['history']
        print('History:', history['blobs_scanned'], 'blobs,', history['commits_scanned'],
              'commits,', len(history['flagged_objects']), 'flagged objects;',
              history['commits_with_identity_metadata'], 'commits contain author/committer identities.')
    raise SystemExit(1 if flagged else 0)


if __name__ == '__main__':
    main()

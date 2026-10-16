"""Generate real JSON payload across file-size boundaries; native load/save/reopen, no Python cells."""
import argparse
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime
import json
from pathlib import Path
import shutil
import subprocess
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mib', type=int, default=40)
args = parser.parse_args()
if args.mib < 1:
    parser.error('Expected a positive payload size')
directory = root / 'Artifacts' / ('large-file-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
directory.mkdir()
if shutil.disk_usage(directory).free < (2 * args.mib + 1024) * 1024**2:
    raise RuntimeError('Not enough free space for the disposable source and roundtrip')
path = directory / 'large.ipynb'
chunk = b'x' * (1024**2 - 32)
with path.open('wb') as file:
    file.write(b'{"nbformat":4,"nbformat_minor":5,"metadata":{"test_chunks":[')
    for index in range(args.mib):
        if index:
            file.write(b',')
        file.write(b'"' + ('%031d:' % index).encode() + chunk + b'"')
    file.write(b']},"cells":[{"cell_type":"code","source":"print(42)","metadata":{},"execution_count":null,"outputs":[]}]}')

class Counters(C.Structure):
    _fields_ = [('cb', W.DWORD), ('faults', W.DWORD)] + [(name, C.c_size_t) for name in
        ['peak_working_set', 'working_set', 'peak_paged', 'paged', 'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile']]

memory = C.WinDLL('psapi').GetProcessMemoryInfo
memory.argtypes = [W.HANDLE, C.POINTER(Counters), W.DWORD]
peak = 0
start = time.monotonic()
with (directory / 'native.log').open('w', encoding='utf-8') as log:
    process = subprocess.Popen([str(root / 'Out/Tests/x64/Release/WinipTests.exe'), '--roundtrip-notebook', str(path)], cwd=root, stdout=log, stderr=subprocess.STDOUT)
    while process.poll() is None:
        counters = Counters()
        if memory(int(process._handle), C.byref(counters), C.sizeof(counters)):
            peak = max(peak, counters.peak_working_set)
        time.sleep(.2)
    if process.returncode:
        raise RuntimeError((directory / 'native.log').read_text(encoding='utf-8'))
report = {'input_bytes': path.stat().st_size, 'output_bytes': Path(str(path) + '.roundtrip.ipynb').stat().st_size,
          'seconds': time.monotonic() - start, 'peak_working_set_bytes': peak, 'semantic_roundtrip_passed': True,
          'data': 'Distinct numbered 1 MiB strings in preserved metadata; one ordinary code cell, not executed',
          'artifact': str(directory)}
(directory / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, indent=2))

"""Sanitizer checks of the complete C64 audio driver with hardware calls mocked.

Optional --source PATH runs the same checks against a preserved audio.c, allowing
before/after comparisons without modifying either firmware source.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path)
parser.add_argument('--output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = (args.source or root / 'src/machines/c64/audio.c').resolve()
source_bytes = source.read_bytes()
has_fraction = b'static void audio_clock_fraction(' in source_bytes
harness = root / 'tests/c64/audio_host'
results = []
with tempfile.TemporaryDirectory(prefix='c64-audio-host-') as tmp:
    exe = Path(tmp) / 'audio-test'
    for diag in (0, 1):
        subprocess.run([
            'cc', '-std=c11', '-O2', '-g', '-Wall', '-Wextra',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
            f'-DWEBDIAG={diag}', f'-DAUDIO_SOURCE="{source}"',
            f'-DHAS_AUDIO_CLOCK_FRACTION={int(has_fraction)}',
            '-I', str(harness / 'include'), str(harness / 'audio_host.c'),
            '-o', str(exe),
        ], check=True)
        result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
        report = json.loads(result.stdout)
        report['webdiag'] = bool(diag)
        results.append(report)
report = {'source': str(source), 'source_sha256': hashlib.sha256(source_bytes).hexdigest(),
          'sanitizers': ['address', 'undefined'], 'runs': results}
text = json.dumps(report, indent=2) + '\n'
if args.output:
    args.output.write_text(text)
print(text, end='')

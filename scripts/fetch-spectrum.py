#!/usr/bin/env python3
"""Prepare checksum-pinned Elite; optional offline input: SPECTRUM_ELITE_FILE."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CONFIG = json.loads((ROOT / 'test-assets/elite48/provenance.json').read_text())
DEST = ROOT / 'test-assets/elite48/ELITE.SNA'

def verify(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() == CONFIG['sha256']

def main():
    if DEST.exists():
        if not verify(DEST):
            raise RuntimeError('Existing Elite snapshot differs from the pinned image; not overwritten')
        print('Spectrum Elite snapshot verified')
        return
    DEST.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=DEST.parent) as directory:
        pending = Path(directory) / 'ELITE.SNA'
        offline = os.environ.get('SPECTRUM_ELITE_FILE')
        if offline:
            shutil.copyfile(offline, pending)
        else:
            archive = Path(directory) / 'elite.zip'
            subprocess.run(['curl', '-fL', '--retry', '3', '--connect-timeout', '30',
                            '--max-time', '180', CONFIG['download'], '-o', str(archive)], check=True)
            with zipfile.ZipFile(archive) as z:
                matches = [i for i in z.infolist() if Path(i.filename).name.upper() == 'ELITE.SNA']
                if len(matches) != 1 or matches[0].file_size != CONFIG['bytes']:
                    raise RuntimeError('Unexpected Elite archive contents')
                pending.write_bytes(z.read(matches[0]))
        if not verify(pending):
            raise RuntimeError('Elite snapshot SHA-256 mismatch')
        pending.replace(DEST)
    print('Spectrum Elite snapshot fetched and verified')

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        raise SystemExit('Spectrum dependency error: ' + str(error))

#!/usr/bin/env python3
"""Fetch the pinned 1581 DOS ROM into ignored roms/, or verify an offline copy."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
NAME = 'dos1581-318045-02.bin'
SHA256 = '8f689277f36be357f5f105e6e606c35c42e73c581cdeec14b58f31850dfbcfcb'
URL = ('https://raw.githubusercontent.com/VICE-Team/svn-mirror/'
       '8e45817287033b36112f6925a89d9f9c953a0cc8/vice/data/DRIVES/' + NAME)
DEFAULT = ROOT / 'roms' / '1581' / NAME


def verify(path):
    data = Path(path).read_bytes()
    if len(data) != 32768 or hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError(f'1581 ROM checksum mismatch: {path}; file has not been overwritten')


def fetch(destination=DEFAULT, source=None):
    destination = Path(destination)
    if destination.exists():
        verify(destination)
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix='.1581-fetch-') as tmp:
        pending = Path(tmp) / NAME
        source = source or os.environ.get('C64_1581_ROM')
        if source:
            shutil.copyfile(source, pending)
        else:
            subprocess.run(['curl', '--fail', '--location', '--retry', '3',
                            '--connect-timeout', '30', '--max-time', '180',
                            URL, '--output', str(pending)], check=True)
        verify(pending)
        # Publish only a fully verified file, without replacing a concurrent writer.
        try:
            os.link(pending, destination)
        except FileExistsError:
            verify(destination)
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=DEFAULT)
    parser.add_argument('--source', type=Path, help='Use a local ROM instead of downloading')
    args = parser.parse_args()
    try:
        print('1581 ROM verified:', fetch(args.output, args.source))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'1581 ROM setup failed: {error}')

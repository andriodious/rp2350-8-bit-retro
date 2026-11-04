#!/usr/bin/env python3
"""Regenerate the checked-in boot logo header. Requires Pillow; not needed to build."""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parents[1]
im = Image.open(root / 'assets/boot-logo.png').convert('RGB').resize((48, 48), Image.Resampling.NEAREST)
data = [(4 if r >= 128 else 0) | (2 if g >= 128 else 0) | (1 if b >= 128 else 0) for r, g, b in im.getdata()]
(root / 'src/common/boot_logo.h').write_text('// Generated from the user-supplied assets/boot-logo.png by tools/embed_boot_logo.py\nstatic const unsigned char boot_logo[48 * 48] = {\n' + ''.join('    ' + ','.join(map(str, data[i:i+48])) + ',\n' for i in range(0,len(data),48)) + '};\n')

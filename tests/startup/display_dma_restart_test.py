"""Run the actual LCD recovery/ISR against a bounded DMA model under ASan/UBSan.

This checks software ordering, pointers and frame timestamps. It does not emulate
RP2350 bus timing, PIO execution or the physical display.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "src/boards/presto/presto_lcd.c").read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


production = function("static void __not_in_flash_func(load_colours)")
production += source[source.index("// Called only in the vertical blank interrupt"):
                     source.index("static void lcd_command")]
production += function("const uint16_t *const *board_display_get_rows")
production += function("void board_display_set_rows")
env = dict(os.environ, ASAN_OPTIONS="halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1")
with tempfile.TemporaryDirectory(prefix="presto-display-dma-") as directory:
    output = Path(directory)
    (output / "display_dma_under_test.h").write_text(production)
    for machine in ("c64", "spectrum", "master", "beeb"):
        for dma_timing in (0, 1):
            binary = output / f"{machine}-{dma_timing}"
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g",
                       "-fsanitize=address,undefined", "-I" + str(output),
                       f"-DMACHINE_{machine.upper()}=1", f"-DPRESTO_LCD_TIMING_DMA={dma_timing}",
                       str(ROOT / "tests/startup/display_dma_restart_test.c"), "-o", str(binary)]
            subprocess.run(command, check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, env=env)
            print(f"{machine}, timing_dma={dma_timing}: {result.stdout.strip()}")

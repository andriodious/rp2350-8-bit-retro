# Experimental Presto FPGA bridge diagnostic

This standalone Pico SDK application exercises the retro8bit_FPGA R2 bridge
from a USB serial terminal. It runs separately from the emulator. It leaves
the panel dark and performs no SD or emulator settings writes. Keep your usual
emulator UF2 available to restore after testing.

[FPGA project and staged bench plan](https://github.com/andriodious/retro8bit_FPGA) ·
[Prebuilt Presto UF2](images/retro8bit_presto_probe.uf2) ·
[Image checksum](SHA256SUMS)

The test uses Presto Qwiic on GP40/GP41 at 100 kHz, with bridge address
`0x32`. The diagnostic sends no bus command at boot. The FPGA starts with
both ports disarmed after configuration. Start with neither Pi attached,
then test Pi1MHz, then PiTubeDirect.

| USB command | Action |
| --- | --- |
| `status` | Read FPGA signature, power, arm state and last result |
| `arm mhz`, `arm tube` | Arm and reset one port when J6 and its supply allow |
| `fcr AA`, `fcw AA DD` | Read or write a FRED address |
| `fdr AA`, `fdw AA DD` | Read or write a JIM address |
| `tr R`, `tw R DD` | Read or write a Tube register, offset 0–7 |
| `halt` | Disarm both ports |
| `bootsel` | Request halt and reboot to USB BOOTSEL |

Arguments are hexadecimal. Use `halt` before switching ports. For the full
procedure, including which signals to scope, use the hardware project's
bring-up plan. This build does not connect bus accesses to the emulator.

## Build

Use CMake, ARM GNU, picotool and Pico SDK 2.2.0 with its TinyUSB submodule
initialized. Export `PICO_SDK_PATH` and put the compiler and picotool on
`PATH`, then run `./build.sh` from this directory. The script builds a
standalone absolute-address UF2. The Presto board definition comes from
this repository's existing board support.

## Reference peripherals

- [Pi1MHz by dp111 and contributors](https://github.com/dp111/Pi1MHz)
- [PiTubeDirect by David Banks (hoglet67) and contributors](https://github.com/hoglet67/PiTubeDirect)

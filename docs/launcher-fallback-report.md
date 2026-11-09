# Launcher fallback report — experimental, 9 November 2026

Purpose: distinguish our launcher's deliberate USB fallback from earlier boot ROM entry or a hang. This is a diagnostic build, not a reset fix. Stable release and reference audio-stop UF2 remain unchanged.

Flash the combined UF2, then power cycle. C64 image ID: `84aa7becf45c48d9`; paired launcher SHA256: `a7a8edcfbceacbcdfd457b13391920d64c1f8affb4be6c4973c1663d5719d83f`.
Keep watchdog enabled, sound off, audio DMA running, Nimbus off with retries enabled. Run C64 Elite for a few minutes and press Reset only once. No need for another prolonged watchdog comparison.

## Outcomes

- Normal display and web return: successful reset, no failure report expected.
- Seven BLUE LEDs repeatedly on/off: our launcher exhausted its machine attempts. It deliberately stays in a reporting loop, with no network/display application running. It should expose a USB serial port, NOT a firmware drive.
- USB firmware drive appears: our final launcher fallback did not intentionally request it in this build; investigate an earlier or other USB boot path. Record the drive name and INFO_UF2.TXT. This does not by itself prove a ROM defect.
- Static LEDs, black screen, no USB: record colour/count and any visible transition. Could be before the reporting loop, or an unsuccessful USB initialisation; absence of serial output alone is not conclusive.

## Read the failure report on a Mac

Connect the data USB cable without cycling power. In Terminal:

```sh
ls /dev/cu.usbmodem*
```
Then substitute the port actually listed:

```sh
screen /dev/cu.usbmodemXXXX 115200
```
The report repeats every two seconds. Copy `LAUNCHER_FALLBACK` and all three `ATTEMPT` lines. Exit screen with Control-A then backslash, confirming exit. Do not power cycle before capturing the report. The diagnostic launcher disables USB baud-rate/vendor reboot commands to avoid a terminal opening triggering BOOTSEL.

`partition` is the table-load return code. `boot_word` and `diagnostic` are ROM boot information captured before our table load. Each attempt includes slot, lookup return code, first/last sectors and chain return code. Chain value 2147483647 means lookup failed so chain was not called. A successful chain does not return. Data is retained in RAM while reporting, not across power loss; there are no diagnostic flash writes.

## Implementation and checks

Opt-in `LAUNCHER_BOOT_REPORT=ON` (requires STARTUP_DIAG). Original USB fallback remains when OFF. Partition load remains on the existing RAM call path. A negative table-load return is allowed to continue into the attempt report instead of the old diagnostic halt. Null ROM lookup/lock failures still halt using existing magenta markers. USB starts only after all machine attempts return; watchdog is disabled at that point to keep evidence available.

Launcher uses Release `-Os -DNDEBUG` for C/C++ to fit USB serial within its existing 32 KiB flash reservation. This changes launcher code layout and is not a controlled timing comparison against v6. No partition relocation. All machines rebuilt to embed the new paired-launcher identity, with existing D81/Wi-Fi/Nimbus code unchanged.

Passed: native mocked ROM tests with address/undefined-behaviour sanitizers (lookup failures, exact return codes, successful handoff); RAM partition-call placement; pre-main wrapper ordering; existing audio-stop/watchdog-toggle tests; combined UF2 slot bounds; git diff whitespace check. Hardware validation pending.

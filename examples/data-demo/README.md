# Cartridge data demo

Loads and retains `assets/message.txt` in Lua. A six-byte binary file verifies
embedded zeros and non-text bytes at startup. Drawing uses the retained text;
A/J performs another read, and B/K demonstrates recoverable `not_found` handling.
See the [data-reader contract](../../docs/api/data.md).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/data-demo -o build/data-demo.vcart -t "Cartridge Data Demo"
./build/sugarbee build/data-demo.vcart
```

## Desktop checks, then Pi

1. Three text lines appear, beginning with "Loaded from the cartridge!".
   The initial status reports the six-byte binary check passed; text-read count is 1.
2. A/J increases the text-read count and reports success. Displayed text stays
   unchanged. B/K reports `not_found`; the game continues without Guru.
3. Pause/resume preserves text and count. Exit Game/relaunch resets the count to
   1 and repeats the binary check. Exit to system works normally.
4. Pong and modules-demo still run normally.

The data files are packaged assets, not external files read from the host.
Repeated reads are deliberately demonstrated, not recommended for a draw loop.

## Validation

Native tests verify read limits, error handling, binary preservation, failed-read
accounting, allocation cleanup and session lifetime. Smoke tests model trusted
directory reads and run the demo for 600 frames.

Desktop: **177/177 serial CTests passed**. Nine native data-reader modes passed
ASan/UBSan with leak detection disabled because of the environment limitation;
no LeakSanitizer acceptance is claimed. The packaged demo ran headlessly for two
seconds without reporting a Guru error; the harness terminated it.
On 2026-11-18, the user confirmed all desktop checks passed: text/binary status,
repeat-read counter, missing-file recovery, Pause/resume, exit/relaunch reset,
Pong/modules-demo regressions and Exit to system. On 2026-11-19, the user
confirmed Pi acceptance at `9d75cd8`: **177/177 tests** and all four requested
checks passed, covering text/binary status, repeat reads, missing-file handling,
Pause/resume, exit/relaunch, Pong/modules-demo and Exit to system.
Desktop and Pi acceptance are complete.

# Native audible stop checkpoint

Internal API: `sb_music_sequence_stop(sequence)` in `src/sb_music_sequence.h`.
Caller must serialize control operations with rendering (for example, SDL's
device lock). No internal locks, allocation, I/O or callback waits are introduced.

## Contract

- NULL returns false. Idle stop succeeds without changing state.
- Stop cancels scheduling and looping immediately. Every active track uses the
  existing synth stop fade: 480 frames / 10 ms at 48 kHz, from its current level.
- State remains FINISHING while rendering drains the fades. Repeated stops do
  not restart/extend them; start remains refused until IDLE.
- Stop during natural finishing applies the stop fade instead of continuing the
  original release. The fade has its existing fixed duration, even if the
  original release would have finished sooner.
- No active voices means immediate silent reset, including before the first
  event, in an authored gap, or exactly at a restored loop boundary.
- Completion resets the bank and snapshot to silent defaults and clears the
  borrowed song reference. The caller-owned asset remains loaded and reusable.
  Keep it alive until idle/reset; there is no handle registry or unload guard.
- Reset remains immediate cleanup. Stop does not advance while the host supplies
  no samples; host-pause integration is explicitly outside this checkpoint.

## Automated checks

`test_music_stop` compares the fade to direct bank-stop output, checks completion
on exactly frame 480, repeated requests, busy refusal, silent reset and replay.
Cases include held notes, release, authored gap, before first dispatch, before/
at/after a loop boundary, natural finishing tails and already-idle playback.
The mixed triangle/noise bank includes an enabled filter. SDL dummy stop smoke
and the complete three-case stop host are checked separately.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build -R '^music_' --output-on-failure
./build/test_music_stop
ctest --test-dir build --output-on-failure
```

Desktop full suite: **97/97 passed** (105.39 seconds); all eight music-focused
checks passed. Five native music ASan/UBSan checks passed with leak detection
disabled under tracing. Debug build had no new warnings. The complete --stop
host also passed with SDL dummy audio. User confirmed all four desktop listening
checks and exit status 0. Pi acceptance reported by user 2026-11-08 at
`0412c37c47099c56793b6221c07f59666dc2eb56`: native stop check confirmed,
97/97 tests passed with exit 0; listening checks passed with exit 0.
Desktop and Pi hashes match. This records user-reported hardware acceptance;
no new local test run was performed for this documentation update.

## Listening acceptance — desktop, then Pi

Start at low speaker volume:

```sh
./build/sbee-music-test --stop
echo $?
```

The tool plays three mixed-instrument cases: stop a held looping chord, stop
natural release tails, and stop in an authored gap. There is a short pause between
cases. Check that stopping is clean without unexpected clicks, bursts or stuck
sound; later cases play normally; no next iteration appears after the gap stop;
and completion/relaunch are silent and repeatable, with exit status 0.

The stop is issued at an exact sample position inside this diagnostic's callback;
the rest of that same output buffer renders the fade, without inserting silence.
This is not host-pause or production threading integration. The caller pauses the
device before fixture data expires. Existing --loop and default one-shot
listening modes remain available for regression checks.

Record tested hash, full-suite result, listening acceptance and exit status.
After acceptance, hand back to SugarBeats for one bounded native consumer stop/
replay/ownership check, not new preview controls or other lifecycle layers.

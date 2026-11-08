# Native whole-song loop checkpoint

Contract: [music loading / native looping](../docs/specifications/music-loading.md).
Core fixtures: [test_music_loop.c](test_music_loop.c).

`start_loop` validates and prepares outside rendering. One-shot `start` is
unchanged. Caller-owned song data remains immutable/alive until idle/reset;
a loop never naturally becomes idle. No stable ABI is promised; rebuild consumers.

Automated coverage:
- 64 iterations exactly equal concatenated fresh one-shot renders, including
  noise, pulse, filter, retrigger, zero release and end-frame events.
- Split buffers equal whole buffers; ending exactly on a boundary restores the
  bank, and the next call starts frame-zero events without an extra sample.
- Busy/invalid/ineligible refusal preserves the entire sequencer; ineligible
  songs still play once. Invalid output and zero-frame renders preserve state.
- One-frame silent loops remain bounded; cleanup resets to silent idle.
- SDL dummy host completes four short iterations and closes cleanly.

Desktop snapshot size: 3640 bytes; total sequence: 7320 bytes. Pi should report
its sizes and test results; do not assume this establishes real-time capacity for
arbitrary workloads or expanded voice counts.

## Build and automated validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build -R '^music_' --output-on-failure
./build/test_music_loop
ctest --test-dir build --output-on-failure
```

Desktop validation 2026-11-08: full suite **95/95 passed** (104.83 seconds),
all six music-focused checks passed, no new build warnings. Four native music
ASan/UBSan checks passed in the host-free build with leak detection disabled
under tracing. SDL dummy loop smoke passed.
Desktop acceptance reported by user 2026-11-08: all listening checks passed,
95 tests passed, exit status 0. Pi automated/listening acceptance remains pending.

## Desktop and Pi listening acceptance

```sh
./build/sbee-music-test --loop
echo $?
```

Start with low speaker volume. Four identical two-second mixed-instrument passes
play major then minor chords with authored release/gap spacing (about 8 seconds).
Check:
1. Each pass has the same rhythm, balance and timbre.
2. Boundary spacing repeats consistently, without an added delay or missing onset.
3. No unexpected click, burst or stuck tone at the repeat seam.
4. Program completes into silence, returns 0, and behaves the same on relaunch.

The host terminates at a known eligible iteration boundary using immediate
cleanup, not a new fade/stop feature. An authored gap is expected: this test does
not claim a continuous sustained note across boundaries.

Report candidate hash, native output/sizes, full-suite totals, listening checks
and exit status. Do not hand back to SugarBeats until required acceptance is
recorded. Then the bounded follower task is native loop parity/ownership checks
against the same pin, not an editor or new runtime lifecycle layer.

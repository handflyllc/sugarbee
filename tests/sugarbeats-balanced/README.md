# SugarBeats per-track levels: cartridge acceptance

2026-11-10 checkpoint. Starting checkout `aa65998`; runtime unchanged since
`e2d7b7c`. No engine runtime or SugarBeats source changes here.

`song.json` is an unchanged snapshot of SugarBeats
`examples/balanced-export/song.json`. Three static instrument levels:
triangle 0.4, saw 0.2, pulse 0.15 (width 0.5). Notes, timing, envelopes, centered
pan and bypassed filters match the accepted mixed export. This does not change
runtime master gain or apply normalization. Original C–F–G–C exercise, 24 events,
120 BPM / 32 ticks, end frame 192000 at 48000 Hz (four seconds), loop eligible.
SugarBeats reports 3207 JSON bytes and 1344 prepared bytes.

Artifact SHA-256:
`f3beb5dd5fb748abdf375d21800460ed6d9d0eff7a7eea0f05a7a445e8576f27`

CSV SHA-256:
`799e13b10bea88fc230a9d98516d7322e9d729013a19c00b260f4b5973913bdc`

Source and exporter remain in SugarBeats. Reproduce from its root to temporary
output, preserving accepted artifacts:

```sh
python3 tools/export_song.py examples/balanced-export/notes.csv /tmp/balanced-song.json --track-2 saw --track-3 pulse --track-1-volume 0.4 --track-2-volume 0.2 --track-3-volume 0.15
```

SugarBeats reports warning-free build, 11/11 serial tests, and four desktop
preview listening checks accepted at clean engine build pin `5eb43a1`.
Its tests include finite bounds, invalid input preserving output, unchanged prior
artifacts, isolated-track scaling, all-zero silence, native eligibility and
render parity/lifecycle. These are follower results, separate from cartridge
and hardware acceptance below.

## Engine validation

Hash-checked packaging copies the snapshot to `assets/backing.json` and reuses
unchanged Music Lab Lua and `test_music_lab`. Device/unavailable-device checks
cover exact one-shot/two-loop boundaries, master gain/mute, independent effect,
stop, host pause/freeze/resume, paused exit and relaunch. Previous fixtures remain.

```sh
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build -R '^sugarbeats_balanced_' --output-on-failure
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

Desktop evidence (2026-11-10): warning-free build; **129/129 serial tests
passed**, exit 0 (150.35 seconds). Fresh export and packaged member match snapshot
bytes/hash; source CSV hash verified. Comparison confirms only static instrument
volumes differ from the accepted mixed asset. Prior artifact hashes unchanged.
Diff check passed. No new sanitizer run (runtime unchanged). Desktop cartridge
listening accepted per the user on 2026-11-10.

Pi acceptance reported by the user on 2026-11-10 at synchronized commit
`49931402793755edaa9d3d2a357a2363c6516690`: **129 tests passed**, exit 0;
all listening checks passed. This completes cartridge acceptance.
Acceptance documentation only; tests were not rerun.

## Listening checklist and Pi reproduction

```sh
./build/sugarbee build/sugarbeats-balanced.vcart
```

The screen retains Music Lab's title and controls:

1. J: one-shot, then compare with `build/sugarbeats-mixed.vcart`. Saw and pulse
   should be less prominent; notes, timing and rests should be unchanged.
   Natural completion and replay are clean, without stuck notes or distortion.
2. W then J while idle: loop for at least three passes. Balance/rests remain
   consistent, without unexpected seam noise.
3. I: independent effect. U cycles music master volume/mute without altering
   that effect. K stops only music; I releases the effect. Idle muted start has
   no opening burst. These controls are separate from exported track levels.
4. Enter: pause/resume with music and effect active, exit while paused, relaunch
   and verify idle defaults and fresh playback.
5. Replay `build/sugarbeats-export.vcart`: original triangle sound/timing unchanged.

To reproduce on Pi, synchronize the checkpoint being validated, then run
serial tests and listening:

```sh
git pull --ff-only origin feature/native-music-sequencer
git rev-parse HEAD
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
./build/sugarbee build/sugarbeats-balanced.vcart
```

Expected suite: 129 tests. Record synchronized hash, count, exit status and
listening result. CTest stays serial on both machines. No merge, new runtime
behavior or composer expansion. Return accepted evidence to SugarBeats, or stop
for a demonstrated mismatch. Canonical passdown remains read-only here.

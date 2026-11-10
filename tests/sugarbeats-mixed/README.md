# SugarBeats mixed instruments: cartridge acceptance

2026-11-10 checkpoint. Starting checkout `826e079`; runtime unchanged since
`e2d7b7c`. No engine runtime or SugarBeats source changes in this checkpoint.

`song.json` is an unchanged snapshot of SugarBeats
`examples/mixed-export/song.json`. Three tracks use triangle, saw and pulse;
pulse width is explicitly 0.5. Existing fixed envelope, filter, volume and pan
settings are preserved. Original C–F–G–C exercise, 24 events, 120 BPM / 32 ticks,
192000 end frame at 48000 Hz (four seconds), native loop eligible.
SugarBeats reports 3206 JSON bytes and 1344 prepared bytes.

Artifact SHA-256:
`bef13e743fbac100e409c1f19ebc0e65499a1cef43dc3e53faa97982f83b0377`

Source CSV SHA-256:
`799e13b10bea88fc230a9d98516d7322e9d729013a19c00b260f4b5973913bdc`

Source, exporter and editable presets remain owned by SugarBeats. Reproduce from
its root with temporary output (do not overwrite accepted artifacts):

```sh
python3 tools/export_song.py examples/mixed-export/notes.csv /tmp/mixed-song.json --track-1 triangle --track-2 saw --track-3 pulse
```

SugarBeats reports a warning-free build, 11/11 serial tests, and four accepted
desktop preview listening checks against clean engine pin `5eb43a1`. Its expanded
checks cover preset combinations, deterministic exports, unchanged default bytes,
invalid presets, loader/playback and mixed one-shot/loop/stop sample parity.
Preview acceptance is separate from cartridge/hardware acceptance below.

## Engine checks

Hash-checked packaging copies the snapshot as `assets/backing.json` and reuses
unchanged Music Lab Lua. Existing `test_music_lab` runs with expected end frame
192000: one-shot boundary, two exact loop boundaries, gain/mute, independent
voice 9, stop, pause/freeze/resume, paused exit and relaunch. Both dummy-device
and unavailable-device cases run. Previous first/timing fixtures are preserved.

```sh
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build -R '^sugarbeats_mixed_' --output-on-failure
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

Desktop evidence (2026-11-10): warning-free build, **126/126 serial tests
passed**, exit 0 (150.15 seconds). Fresh export and packed member match snapshot
bytes/hash. CSV hash verified; previous artifact hashes unchanged. Diff check
passed. No new sanitizer run (runtime unchanged). Desktop cartridge listening
and checks accepted per the user on 2026-11-10, exit 0. Pi validation remains
pending; changes await development-host commit/push.

## Desktop listening and Pi handoff

```sh
./build/sugarbee build/sugarbeats-mixed.vcart
```

The screen still says Music Lab. Use its existing controls:

1. J: one-shot; listen for brighter mixed chords and intentional short rests,
   silent natural completion, and clean replay. No stuck notes or unexpected distortion.
2. W then J while idle: loop for at least three passes; chords and rests repeat
   consistently without unexpected seam noise.
3. I: independent pulse effect. U cycles music volume/mute; effect stays audible.
   K stops only music. I releases the effect. An idle muted start has no burst.
4. Enter: pause/resume repeatedly with music and effect active; exit while paused,
   relaunch and verify idle defaults and fresh playback.
5. Launch `build/sugarbeats-export.vcart` and confirm the original triangle asset
   retains its accepted sound/timing.

After user desktop acceptance, commit/push from the development machine. On Pi,
synchronize the exact new checkpoint commit and repeat serial tests and listening:

```sh
git pull --ff-only origin feature/native-music-sequencer
git rev-parse HEAD
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
./build/sugarbee build/sugarbeats-mixed.vcart
```

Expected suite: 126 tests. Record synchronized hash, count, exit status and
listening outcome. CTest remains serial on both machines. No merge, new runtime
behavior or composer expansion. Return accepted evidence to SugarBeats, or stop
for a demonstrated mismatch. The canonical passdown remains read-only here.

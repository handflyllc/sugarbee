# SugarBeats configurable timing: cartridge acceptance

2026-11-10. Selected 137 BPM / 40-tick export, preserved unchanged from
SugarBeats `examples/timing-export/song.json`. Runtime remains `e2d7b7c`;
starting checkout `cbf3bb9` includes the accepted first-export fixture and docs.
No runtime, exporter, preview or editor changes in this checkpoint.

SHA-256: `5275d42154021993c4f45c3467c02d2f2e08e5ea8dea126d0fb9a32946cbe8b1`

Three triangle tracks, 12 notes / 24 events, 48000 Hz timeline, end frame
210219 (4.3795625 seconds), native loop eligible. The original notes end at tick
31; the declared end is tick 40. This intentional ending rest is longer than
in the first export. Event positions use absolute rational rounding, not a
repeated rounded tick length. The original accepted fixture is unchanged.

Source and reproduction remain owned by SugarBeats:

```sh
# From the SugarBeats root; choose a temporary output to preserve the artifact.
python3 tools/export_song.py examples/first-export/notes.csv /tmp/timing-song.json --bpm 137 --end-tick 40
```

SugarBeats reports 9/9 serial checks passed, including rounding and native
eligibility tests, against engine pin `5eb43a1`. Its separate 240 BPM ineligible
case is not part of this selected cartridge checkpoint. No additional follower
listening or hardware evidence is implied.

## Engine validation

The pack fixture checks the hash and reuses unchanged Music Lab Lua. The shared
host test accepts an expected end frame (default 192000 for previous fixtures).
One-shot is still playing one frame before the declared end, then idle at the
end. Loop tests check the exact boundary and a second iteration before continuing
100 frames. Existing gain/effect/stop/pause/exit/relaunch and unavailable-device
checks run on the selected asset. No engine rendering behavior changes.

```sh
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build -R '^(music_lab|sugarbeats_export|sugarbeats_timing)_' --output-on-failure
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

Desktop evidence (2026-11-10): warning-free build; **123/123 serial tests
passed**, exit 0 (150.01 seconds). Original Music Lab and first-export checks
remain passing alongside the three new timing-export checks. Fresh reproduction
and packed member match the handoff bytes/hash. Original fixture hash unchanged.
Diff/whitespace check passed. No new sanitizer run; runtime unchanged.
Desktop listening accepted per the user on 2026-11-10. Pi validation remains
pending; changes await development-host commit/push.

## Listening and Pi handoff

```sh
./build/sugarbee build/sugarbeats-timing.vcart
```

The screen still says Music Lab. Repeat the
[existing five-step checklist](../music-playback.md#desktop-and-pi-listening-checklist):
one-shot/replay, loop, gain/mute with independent effect, stop/muted start,
pause/resume/exit/relaunch. Expect faster chord changes than the 120 BPM export
and a longer authored rest at the end of each 4.3795625-second loop. That rest
is not a stalled timeline or an extra fade inserted by the engine.

After desktop listening acceptance, commit/push on the development host. On the
Pi, synchronize the exact checkpoint commit before building and testing:

```sh
git pull --ff-only origin feature/native-music-sequencer
git rev-parse HEAD
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
./build/sugarbee build/sugarbeats-timing.vcart
```

Expected suite: 123 tests. Report synchronized hash, count, exit status and
listening outcome. Keep CTest serial on both machines. No parallel Pi retest,
merge or additional engine/composer scope is included.

## Required note for the next SugarBeats passdown

SugarBeats README still contains the obsolete “No GUI, exporter, or JSON-driven
audible preview” status beneath its historical heading. The exporter now exists.
Shorten/remove that stale status or date it clearly so it cannot be mistaken for
current capability. Preserve the distinction: GUI and JSON-driven audible preview
remain deferred. User explicitly requested this note in the next passdown.
The canonical passdown is read-only from this session; no second copy is created.

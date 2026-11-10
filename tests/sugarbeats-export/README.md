# First SugarBeats export: cartridge acceptance

2026-11-09 checkpoint. Engine runtime remains `e2d7b7c`; starting checkout
`1ce1e83` adds only documentation. No runtime or SugarBeats code changes here.

`song.json` is an unchanged snapshot of SugarBeats
`examples/first-export/song.json`, exported from its `notes.csv` with
`tools/export_song.py`. This copy makes engine/Pi tests self-contained; the
exporter and editable note source remain in SugarBeats. The shared coordination
file remains exclusively `sugarbeats/docs/PASSDOWN.md`.

SHA-256: `962f729a5d5e598cb115653dba8d97db8f01f1f3c5f0527a45514f2f7b117c77`

Original C–F–G–C exercise: three fixed triangle instruments, 12 notes/24 events,
120 BPM, 192000 frames at 48000 Hz (four seconds). JSON: 3186 bytes. SugarBeats
reports native prepared storage 1344 bytes and loop eligible. Each chord has an
intentional rest: notes end 125 ms before the boundary, with 50 ms release.

SugarBeats reports 9/9 serial checks passed against `5eb43a1`, including export
repeatability/refusals/order and native one-shot/two-loop checks. Its existing
consumer regression counted 264 allocations matched frees; that accounting does
not cover the new export test. No follower listening/Pi claim.

## Engine checks

The pack fixture verifies the asset hash, copies its bytes as `assets/backing.json`,
and reuses the unchanged Music Lab `main.lua`. Packaging uses the real packer.
Both device and unavailable-device cases reuse `test_music_lab`: one-shot,
looping, gain/mute, independent voice 9, stop, pause/freeze/resume, paused exit
and relaunch. Music Lab's init queries loop eligibility through the Lua API.
No parallel SugarBeats checkout or exporter installation is needed on the Pi.

```sh
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build -R '^sugarbeats_export_' --output-on-failure
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

Desktop evidence (2026-11-09): warning-free build; **120/120 serial tests
passed**, exit 0 (149.69 seconds), including all three new export-cartridge checks.
A fresh export to `/tmp` independently reproduced the supplied CSV's artifact;
the packed cartridge member also matches the snapshot bytes and hash. Diff check
passed. No new sanitizer run (runtime code unchanged). Desktop listening passed
per the user on 2026-11-09. Pi validation remains pending; changes await the
development-host commit/push. SugarBeats source untouched. Pause before Pi execution.

## Desktop listening, then pause before Pi validation

```sh
./build/sugarbee build/sugarbeats-export.vcart
```

The screen retains the Music Lab title and controls; this cartridge contains the
exported four-chord exercise instead of the original two-chord lab asset.
Use the [five-step Music Lab checklist](../music-playback.md#desktop-and-pi-listening-checklist):
one-shot/replay, looping and harmony, gain/mute with independent effect, stop and
muted start, host pause/resume/exit/relaunch. Expect authored rests, not seamless
sustained audio. Desktop listening requires user acceptance.

After desktop acceptance, commit/push this fixture/documentation checkpoint from
the development machine. **Pause before Pi execution.** When ready later, verify
Pi is on the same feature branch and synchronize the exact new commit:

```sh
git pull --ff-only origin feature/native-music-sequencer
git rev-parse HEAD
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
./build/sugarbee build/sugarbeats-export.vcart
```

Expected suite: 120 tests. Repeat the listening checklist on available HDMI output;
report commit, count, exit status and listening result. CTest remains serial;
parallel Pi test lockups are unresolved. No merge, new engine feature, GUI or
preview expansion is authorized. Hand back to SugarBeats after recorded acceptance.

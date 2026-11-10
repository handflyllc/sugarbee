# Lua music playback and Music Lab checkpoint

2026-11-09: desktop and Pi automated/listening checks accepted at `e2d7b7c`.
Predecessor runtime `4910b38`, acceptance docs `1d6889f`; SugarBeats review at
that docs pin passed 7/7, with 264 allocations matched frees and no mismatch.

## Scope and automated checks

[API contract](../docs/api/music.md): play/state/stop/set_volume over the existing
native player, explicit complete voice map, atomic initial volume. No schema,
limits, cartridge format, synth or WAV API changes. No composer changes or merge.

`test_lua_music` exercises raw options, type/range/arity checks, foreign/stale
handles, unavailable audio, busy player/voices, ineligible loops, protected
borrows, copied mapping, exact muted output from the first callback, gain ramp
and repeat target, loop gain preservation, default reset, natural completion,
independent effect survival, held starts and stop during pause. Prior loading,
allocation-failure and real-host lifecycle checks remain.

`test_music_lab` drives the actual example with SDL input in device/unavailable
modes. It checks one-shot completion, looping, music mute with independent effect,
host freeze/resume, stop, paused exit and fresh relaunch. Dummy-device checks do
not establish musical quality or hardware output quality.

```sh
cmake --build build -j4
ctest --test-dir build -R '^(lua_music|music_lab)_' --output-on-failure
ctest --test-dir build --output-on-failure
```

Desktop evidence (2026-11-09): warning-free build; **117/117 tests passed**,
exit 0 (37.39 seconds, four workers). All **36 selected checks in the ASan/UBSan
build passed**, including fixture setup and Music Lab/device-unavailable cases
(30.19 seconds, two workers). Leak detection was disabled; no LeakSanitizer result
is claimed. Diff/whitespace review passed. Desktop listening passed per the user on
2026-11-09. Pi acceptance reported by the user on 2026-11-09 at synchronized
`e2d7b7c7afd0d5fe7262f13e99a01f714bae9d10`: **117/117 tests passed**, exit 0,
and all validation/listening checks passed. Runtime is committed; acceptance
documentation awaits commit/push. Feature remains unmerged. SugarBeats code and
the read-only canonical passdown were not modified.

Pi execution constraint: the user reports full-system lockups with CTest `-j2`,
`-j3` and `-j4`, under both fish and bash, with no TTY access and reboot required.
Serial CTest passed. Use the serial command above on Pi; parallel compilation
with `cmake --build build -j4` remains usable. CTest normally executes tests;
it does not compile the project in this invocation (fixtures may prepare assets).
The lockup cause is unresolved; serial success does not establish a fix or a
particular audio, driver, kernel, resource or test-concurrency diagnosis.
Do not repeat parallel Pi CTest as part of routine acceptance. Investigation is
separate future work; no new implementation is authorized by this observation.

## Desktop and Pi listening checklist

Build/pack/run using [Music Lab instructions](../examples/music-lab/README.md).
The two chord phrases include short rests. Listen at low volume:

1. J starts the pattern; let its four seconds finish. It becomes idle and can
   start again. Press J during playback: `music_busy`, with no restart.
2. W enables looping for the next start. J starts it; hear several iterations.
   Check intervals/chords for objectionable roughness or unexpected transients.
3. I adds the sustained pulse effect. U cycles music through .3, mute, .8:
   music fades smoothly; the effect remains. Muting does not stop time.
4. K stops only music. Effect continues. I releases it. A new J starts normally.
   At idle select mute, then J: no opening burst. U restores audible playback.
5. With looping music and effect active, Enter pauses; wait, resume, repeat.
   Both fade/freeze/resume without unexpected blips. Exit while paused and
   relaunch: idle, no lingering sound, default settings, fresh playback works.

Report desktop listening acceptance first. Then commit/push on development host,
synchronize the exact commit on Pi, rebuild, run targeted/full checks and repeat
listening on the available HDMI output. Record hash, count, exit status and
listening observations. After acceptance, update docs and the canonical SugarBeats
passdown for bounded follower review; do not advance composer/engine scope here.

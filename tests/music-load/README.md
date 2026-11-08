# Bounded in-memory music loading

Internal target: `sugarbee_music_load`; header: `src/sb_music_load.h`.
Contract: [music-loading.md](../../docs/specifications/music-loading.md).

The native test reads the checked-in fixtures, mutates copies for malformed
schema/syntax/range checks, builds maximum-event and memory-stress inputs, and
forces allocation failure at every malloc call in small/large loads. Fixtures
are source files, not cartridge assets installed by the build.

Valid fixtures cover one note, a mixed chord, same-frame retrigger, deterministic
noise, silence, nine unused tracks and a one-frame duration. The native test
compares chord/noise output to independently authored C songs, exercises split
buffers and replay, and destroys original input before playback. Failure tests
include decoded duplicate keys, UTF-8/surrogate failures, missing/unknown fields,
wrong types, invalid integer notation and overflow, pairing/order failures,
pulse applicability, ranges, depth, input size, track/event counts and parser
memory exhaustion. Prepared storage remains below its cap by construction at
the event ceiling; the guard is retained for future structure/limit changes.

## Commands

From the SugarBee repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build -R '^music_load$' --output-on-failure
./build/test_music_load
ctest --test-dir build --output-on-failure
```

The same commands apply on Pi after fetching the candidate commit. Confirm
`git rev-parse HEAD`, report suite totals and the printed allocation figures.
No new listening program is introduced: this loader cannot open an audio device.
Existing sequencer listening acceptance is preserved; sample parity tests verify
that the loaded fixtures produce the same samples as the native fixtures.

## Desktop evidence — 2026-11-08

- Debug loader check passes; no new compiler warnings.
- ASan/UBSan loader check passes, `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer
  disabled because of the existing tracing limitation; wrapped allocation
  accounting checks cleanup separately, including every injected allocation failure.
- Three-track chord: parser peak 6202 bytes, prepared 952 bytes.
- 4096 events: parser peak 1271499 bytes, prepared 99064 bytes.
- Figures are requested allocation bytes, not process RSS or final Pi measurements.
- Full desktop suite: **92/92 passed**, 104.17 seconds. Additional track-boundary
  test cases added during that run passed the targeted and sanitizer reruns.
- Pi acceptance reported by the user on 2026-11-08 for the prepared candidate
  `10c3d805188adc17d95a7f22192986d38951d34c`: direct loader check exit 0;
  full suite 92/92 passed, exit 0. Pi allocation figures were not supplied.
  This is user-reported hardware evidence, not a new local test run.

## Consumer usage

Call `sb_music_load(bytes, length, &result)` outside the callback. A NULL return
publishes no asset; `result.code/message` explains refusal and syntax/depth errors
include a byte offset. Array indices in diagnostics are zero-based (track IDs in
the asset are one-based). On success, discard the JSON bytes if desired and pass
`sb_music_asset_song(asset)` to the existing sequencer. The asset must remain
alive and immutable until playback is idle or reset. Then call
`sb_music_asset_free(asset)`. There is no runtime active-unload guard in this
internal C interface. No code reads parser objects during rendering.

Do not treat a successfully loaded song as loop-eligible; loop analysis and
playback, shared voice reservations, runtime handles, Lua and cartridge reads
remain separate checkpoints.

# SugarBee → SugarBeats: validated engine status and independent continuation

Updated 2026-11-21. This transfer note supersedes the 2026-11-16 pitch-range
integration request previously in this file. SugarBee's agreed checkpoints are
complete; no composer change is required by this handoff. SugarBeats owns its
current dependency pin, roadmap and consumer validation. Its current checkout
was not inspected or modified during this update.

## Engine revisions and acceptance

- Current engine-repository reference: **1fb9542653689b32dc383b1bdd4b7f2d125bbf1c**
  (`1fb9542`), pushed by the user. The latest change shelves the glyph-sheet
  example; it is documentation only.
- Latest implementation checkpoint: **953a2026b865dd2655bd07e14228d382d5dbf855**
  (`953a202`), the cartridge-only three-pane interaction demo and tests.
  Desktop: warning-free build, **204/204 serial tests**, all five manual checks.
  Pi: user confirmed **204/204 tests and requested manual validation passed**
  on 2026-11-21. Acceptance documentation was pushed as `f21910e`.
- Latest relevant audio API addition: **f37ed755e84274ddff62bf08335327dc04f828fd**
  (`f37ed75`), direct Lua synth frequency control. Desktop and Pi accepted;
  Pi 193/193 tests and requested checks passed. This exposed existing native
  frequency control; it did not change waveform generation or the music format.
- Historical A0–C8 implementation: `4d7ca11`; acceptance reference `7085043`.
  That integration request is no longer the next task for the composer.

The latest 204-test acceptance was performed at `953a202`, not freshly rerun at
its documentation-only successors. These results do not replace SugarBeats'
consumer tests or establish acceptance of every possible exported composition.

## Last known SugarBeats status

The last returned report recorded in SugarBee describes engine pin `7085043`,
17/17 consumer checks, A0–C8 support and one through nine tracks accepted at
composer code `0851633` / documentation `08e841d`. Treat this as historical
context, not a claim about the present SugarBeats checkout. Inspect its current
pin and `docs/PASSDOWN.md` before deciding what remains to do. Do not repeat
already completed range or track-management work based on the old transfer note.

## Audio contracts relevant to the composer

- Pitched triangle, saw and pulse accept finite **27.5–4186.01 Hz inclusive**
  (A0–C8). Fractional frequencies are preserved; no snapping, clamping or extra
  tolerance. Exact equal-tempered C8 (~4186.009044809578 Hz) and 4186.01 Hz are
  both valid. Note-to-frequency conversion belongs to the composer.
- **Nine synth voices**, existing mixing gains and **48 kHz** output remain.
  Declared music tracks reserve mapped voices even when empty or export-disabled;
  do not assume those slots are available for direct synth effects.
- Music assets retain one through nine tracks, fixed per-track instruments and
  the existing format/timing rules. No new dynamic-pitch event format was added.
  Current limits include 1 MiB input, 4,096 events, 256 KiB prepared storage per
  asset, eight loaded assets and 1 MiB total prepared music storage.
- `sugarbee.synth.set_frequency(voice, hz)` changes an active pitched voice while
  preserving oscillator phase and envelope progress. It does not retrigger a
  note, start an idle voice or schedule a glide. Idle/noise/music-reserved voices
  are refused. This is a game-facing Lua API, not an instruction to add composer
  automation or exported pitch envelopes.
- Noise has no new pitched-noise/rate control. ADSR and pulse-width contracts
  remain as documented. Cartridge/music formats are unchanged by this handoff.

Authoritative details: `docs/api/audio.md`, `docs/api/music.md` and
`docs/specifications/music-loading.md`. Consult the actual headers and linked
library targets when intentionally updating the consumer dependency.

## Parked audio ideas — no implementation requested

The user considers these noncritical and is returning to SugarBeats cleanup and
then game authoring:

- Percussion experiments using existing waveforms and amplitude envelopes;
  consider a small pitch envelope only if experiments establish a need.
- Direct-versus-smoothed frequency listening comparisons. Audible stepping and
  a digital character were accepted for direct control; smoothing is not present.
- Editable/resettable presets, noise/envelope editing and dynamics remain
  composer design discussions unless a specific engine requirement emerges.
- Twelve voices remain a proposal, not an accepted capacity increase. Do not
  change voice count, mixing gain or sample rate as part of routine cleanup.

## Suggested continuation in SugarBeats

1. Read the composer's current handoff, working-tree state and dependency pin.
   Continue its own agreed loose ends; this note adds no mandatory feature work.
2. Retain the existing pin unless a deliberate update is useful. If updating,
   `1fb9542` is the current engine reference: rebuild linked libraries and run
   the composer's native consumer, project reload, export and preview regressions.
   Record the exact chosen pin and any required listening results.
3. Preserve existing project/export compatibility. Report a concrete engine
   defect, contract mismatch or required capability back to SugarBee with a
   reproducible example. Routine GUI work needs no new engine/Pi acceptance cycle.

SugarBee and SugarBeats remain independent. No composer files, dependency pins,
exports or external repositories were changed by this handoff.

## Engine context and validation limits

Since the earlier pitch handoff, SugarBee has accepted graphics primitives,
clipping/camera/tint/opacity/rotation, canvas selection, cartridge-local modules
and data reads, text measurement/wrapping/entry, save deletion, packaging identity,
statistics and a three-pane command-driven demo. These do not require composer
integration. Glyph design and cell dimensions belong to game authors; the glyph
example is shelved until further notice.

Serial CTest remains the policy on desktop and Pi. Historical full-system
lockups have no established cause. An intermittent module-fixture timeout was
followed by successful rechecks; its allowance was increased from 10 to 20 seconds
at `9f2665e` and validated on Pi. The original delay was not diagnosed as storage
failure. These observations are not new audio defects or reliability guarantees.

SugarHive is the working name for a future separate bootable console-environment
repository. It will package a selected SugarBee revision; OS/configuration,
updates and recovery belong there. No implementation or composer integration is
requested. The user plans to open that discussion after SugarBeats work.

Run engine tests serially:

```sh
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

Parallel compilation remains allowed. Pi validation belongs to SugarBee;
SugarBeats retains its independent Linux x86_64 scope.

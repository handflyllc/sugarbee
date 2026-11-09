# Native host pause checkpoint

Internal adapter: `src/sb_music_host.h`. This proves host lifecycle behavior with
an exclusive music bank; it does not connect music to the production mixer,
reserve shared voices, or expose a Lua/cartridge API. Production session audio
and the adapter use the same extracted 480-frame host fade calculation.

## Contract

- Initialize with `sb_music_host_reset`; this gives silent idle, unpaused host
  state and host gain 1. Reset is also immediate teardown with callback excluded.
- Start uses existing atomic one-shot/loop validation. The caller keeps song,
  instruments and events alive until idle/reset. Controls and render must be
  serialized. Call the adapter's start/stop/render; direct sequence start/stop
  would bypass host lifecycle rules. Sequence gain controls and state queries
  remain usable under the same synchronization.
- Pause is host state, separate from idle/playing/finishing. Fade out over 480
  rendered frames (10 ms), then emit silence without advancing song events,
  oscillator/filter/noise/envelope state, or the music gain ramp.
- Resume fades in over 480 frames and continues the frozen playback. Apply the
  current fade value to both channels before advancing it for the next sample.
  Reversals ramp from the current value; repeated identical pause requests do
  not extend a fade. Music gain remains independent of the host fade.
- Stop while fully paused immediately resets the sequence and releases its
  borrowed song reference. Host pause remains in effect.
- Stop during pause fade immediately cancels events/looping through native stop,
  and latches cleanup no later than host silence. Voice stop fades may complete
  earlier. Resume before silence never cancels the stop: it drains to idle.
- Starting while pause is requested, including during fade-out, holds playback
  at frame zero with no event dispatch. It still reports playing and refuses
  another start. Resume permits opening events. Stop before resume cleans up
  immediately because the held playback has never sounded.
- Zero-frame render dispatches nothing. Invalid buffers refuse without mutation.
  Frozen rendering still permits control calls; a gain request can set a target,
  but its ramp does not advance until playback resumes.

Shared voices, failure/exit integration with cartridge music, and independent
music pause remain outside this checkpoint. The adapter has no allocation,
parsing, SDL, Lua, or I/O. The existing sequencer layout/contract is unchanged.
No stable ABI is promised for these internal interfaces.

## Automated validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
./build/test_music_pause
ctest --test-dir build -R '^(music_|audio_session)' --output-on-failure
ctest --test-dir build --output-on-failure
```

The native pause test compares samples to a continuously rendered sequence plus
an independent host fade formula. It verifies whole/single-frame equivalence,
full frozen-state equality, music gain freezing, loop crossings, both fade
reversals, held starts, atomic refusal, stop during/after pause, resume during
stop, natural release tails, and reset cleanup. Existing production session
fade/output tests protect the shared calculation extraction.

Desktop Debug build: no warnings. Full suite: **101/101 passed** (106.58 seconds).
All 14 music/session checks passed. Seven native music ASan/UBSan checks passed
with leak detection disabled under tracing. Both the short SDL dummy smoke and
the complete nine-second pause script exited successfully. These are automated
checks, not listening acceptance. User subsequently confirmed desktop listening
acceptance for both `sbee-music-test --pause` and `sbee-session-test`, with exit
status 0 for both (2026-11-08, America/New_York). Pi acceptance remains pending.

## Listening acceptance (desktop, then Pi)

Start with low speaker volume:

```sh
./build/sbee-music-test --pause
echo $?
```

The approximately nine-second script prints each transition. Confirm:

1. Initial playback fades out cleanly and stays silent while paused.
2. Resume continues the song through a clean fade-in, without an opening restart.
3. Stop during pause fade stays silent after the following resume.
4. A new start while paused makes no sound; the next resume begins its opening
   chord. The intermediate held start is silently cancelled and prepared again.
5. Stop fully paused stays silent after resume; completion and relaunch are
   clean, with exit status 0.

Also check the existing host session listening regression, because its fade
calculation is now shared:

```sh
./build/sbee-session-test
echo $?
```

Confirm the printed tone/pause/resume/failure/fresh-session transitions and silent
paused shutdown, with exit status 0.

After desktop acceptance, commit/push the checkpoint and repeat automated and
listening checks on the Pi at the synchronized hash. Then return one tested pin
to SugarBeats for bounded offline host-lifecycle consumer checks.

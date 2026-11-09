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
status 0 for both (2026-11-08, America/New_York). Pi listening acceptance remains open. At synchronized
`c46c17cc6a9084040562152d201940a341b341bb`, the user reported 101/101 tests
passed and exit 0 throughout. All other listening checks passed, but
`sbee-music-test --pause` produced a brief static/pop artifact at "Stop during
pause fade" on the Pi, not on the development machine. Cause and repeatability
are not yet established. Do not treat successful exit status as audible acceptance.
Follow-up: the user tests HDMI0; the artifact is noticeable in `--pause`,
not ordinary `--stop`. Other output paths are unavailable or unsuitable and are
not required for this investigation. Repeatability count and cause remain unconfirmed.
The next diagnostic captures the samples supplied to SDL before changing fades.

## Listening acceptance (desktop, then Pi)

Start with low speaker volume:

```sh
./build/sbee-music-test --pause
echo $?
```

The approximately nine-second script prints each transition. Its revised fixture
uses one sustained chord so a new note after an authored rest cannot be confused
with the host fade. Confirm:

1. Initial playback fades out cleanly and stays silent while paused.
2. Resume restores the held chord through a clean fade-in, without an extra burst.
   Native sample comparisons independently verify frozen-position continuity.
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


## Pi HDMI0 artifact investigation

The diagnostic-only option below runs the same pause script and captures its
post-attenuation float samples. It does not change engine fade behavior. A
20-second bounded buffer is allocated and touched before playback; the callback
only copies into that storage. Overflow fails the diagnostic. File writing
happens after the device closes. The named output file is replaced.

```sh
./build/sbee-music-test --pause-capture /tmp/sugarbee-pause-pi.wav
echo $?
```

Save the console output (SDL backend, callback size and exact transition frames).
Transition 2 is stop during pause fade. Report whether the pop was audible in
this capture run. Copy the WAV to the development machine for listening/sample
inspection. A clean capture with an audible live pop points toward delivery
outside the captured samples; an artifact in the capture points toward generated
audio. Neither result alone establishes the precise cause. Capturing can alter
scheduling, so a run without the live artifact is not a reproduction.

No new Pi listening acceptance or runtime fix is claimed by this diagnostic.


Capture follow-up (user report, 2026-11-08): both saved Pi WAVs reproduce the
blips when played on the desktop; `-01` is more pronounced near three seconds.
This implicates content in the capture rather than HDMI delivery alone; the
exact sample defect is not yet identified. The provided console reports
PulseAudio, 48 kHz stereo, 512-frame callbacks, and stop/pause transition frame
140800. Do not assume that transition frame applies to both files without
checking each capture. Uploaded byte retrieval failed in this session; waveform
inspection awaits locally accessible copies. No fix or audible acceptance claimed.


### Diagnosis and fixture correction

Both uploaded captures are now inspected locally. `-00` contains 431616 frames;
`-01` contains 432128, so the earlier 428544-frame console trace is not the exact
trace for either file. All samples are finite and within range.

Reconstructing the original fixture on desktop with callback-aligned control
positions reproduces **every stereo float sample exactly** in both Pi captures
(zero differing samples). Inferred pause/resume/stop positions are 47616/95744/
143872 for `-00` and 48128/96256/144384 for `-01`. The final pause positions are
335872 and 336384 respectively. These are reconstruction-derived positions,
not a claim that the supplied log contains these markers.

The initial chord naturally releases to silence at frame 41760 (0.870 s).
A new chord begins at 48000 (1.000 s), close to the wall-clock pause request.
In `-01`, the loud burst starts at frame 143649, before the stop request at
144384, and ends at 144864: exactly the end of the 480-frame stop/host fade.
The other bursts likewise end at host fade completion. They are scheduled note
onsets truncated by controls, not sound reappearing after a completed host fade.
Wall-clock controls reach different song positions between runs/platforms.
This evidence does not identify an HDMI, PulseAudio or engine rendering defect.

The pause diagnostic now uses a 30-second held chord in a 31-second eligible
loop; its approximately nine-second script reaches no new note/rest boundary.
No engine fade, stop, scheduling or ownership behavior changed. Capture remains
available. User confirmed revised desktop listening on 2026-11-08
(America/New_York): no audible artifacts, exit 0. Revised Pi listening remains
pending; do not claim its acceptance from inspection of the earlier recordings.

Revised-fixture desktop checks: warning-free diagnostic build; all 14
music/session checks passed. A complete SDL dummy capture passed format/range
inspection and exact-zero checks from each host fade completion to the next
intended resume/start (or end). No audible acceptance is inferred from this run.

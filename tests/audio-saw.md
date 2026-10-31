# Standalone PolyBLEP saw

This pure C component generates normalized mono saw samples at 48 kHz and
accepts finite pitches from 220 through 660 Hz. The oscillator itself has no
envelope, volume, or diagnostic gain. Native bank/SDL integration is described
below; Lua controls remain deferred. Existing listening tools are unchanged.

## State and output contract

- Reset validates pitch and restarts at phase 0.5: the rising zero crossing.
  Zero-initialized storage is unconfigured and must be reset before use.
- Pitch changes preserve phase. They are immediate, not a glide; preserving
  phase does not promise click-free arbitrary frequency jumps.
- Render outputs the current phase, then advances by frequency/48000 and wraps.
  Different buffer partitions produce identical output and ending state.
- Samples stay within ±1 without clipping. A quadratic residual corrects the
  downward discontinuity within one sample interval on either side of the wrap.
- Invalid/nonfinite pitch, invalid state, null arguments, and overflowing sizes
  fail unchanged. Zero samples permit null output but require valid state.
- The caller owns synchronization and output capacity; output must not overlap
  oscillator storage. Reset is initialization, not an audible fade.

The implementation uses the PolyBLEP method: a raw rising ramp plus local
quadratic corrections at its wrap. This suppresses aliasing but does not eliminate
it. Wider frequency support, modulation quality, and bank headroom must be
validated separately before expanding the prototype.

Method references: [sndkit's algorithm explanation](https://paulbatchelor.github.io/sndkit/blep/)
and [Orastron's saw algorithm overview and research references](https://www.orastron.com/algorithms/bw_osc_saw).
These describe the approach; no external runtime library is introduced.

## Automated checks

```sh
ctest --test-dir build -R '^audio_saw$' -V
```

Checks cover reset/replay, known ramp/wrap samples, correction regions,
phase-preserving pitch updates, finite bounds, invalid requests, and identical
output under different buffer partitions. A phase/pitch grid checks bounded
output throughout the supported region.

The spectral regression uses 8192-sample coherent records at 228.515625,
439.453125, and 650.390625 Hz. It compares residual power outside the intended
harmonic bins against the same-phase raw ramp. The corrected signal must reduce
that unwanted power by at least 10 dB while keeping fundamental amplitude within
1% of the raw signal and near the expected 2/pi. Desktop measurements were
16.21, 16.05, and 15.84 dB respectively. This prevents passing by merely reducing
volume. The metric does not count alias components that coincide with intended
harmonics and does not establish quality at all frequencies or during modulation.

Build without the SDL/Lua host:

```sh
cmake -S . -B /tmp/sugarbee-saw-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-saw-standalone --target test_audio_saw
ctest --test-dir /tmp/sugarbee-saw-standalone -R '^audio_saw$' --output-on-failure
```

## Native bank and SDL integration

`sb_audio_voice_saw_on(audio, voice, frequency)` starts or retriggers a saw in
one of the nine native slots. Idle start resets the saw phase to 0.5; active
saw retrigger preserves phase, changes pitch, and attacks from the current level
over the full new attack duration. This also applies during release or short
stop, which retrigger cancels. Pitch-only changes preserve phase and envelope.
The supported range remains 220–660 Hz; no pitch glide is added.

Triangle, noise, and saw share ADSR, volume/pan smoothing, mute, release, and
10 ms stop behavior. Muted saw notes still advance phase, as do release and stop
until idle. A fresh idle start replays the initial phase. Only idle slots can
change waveform type; active cross-type starts fail unchanged. Whole-bank reset
restores silent/default state. Legacy diagnostics refuse concurrent bank play.

Mixing retains fixed 1/9 headroom. Nine coherent maximum-level saw voices are
bounded without clipping; mixed triangle/noise/saw sources retain the same bound.
SDL applies separate bank diagnostic gain 0.790569415, unchanged from the prior
listening tests. Controls lock the device; no allocation, Lua call, or logging
occurs in the callback. Cartridge-owned audio and Lua controls remain deferred.

`audio_saw_bank` checks per-slot routing, replay, active/release/stop retrigger,
pitch-only updates, invalid and cross-type requests, phase progression while
muted, release/stop completion, all-nine headroom at left/center/right, mixed
block partitioning, slot isolation, and reset silence. `audio_bank_sdl` adds
saw controls, mixed callback samples, legacy exclusion, and active teardown.
`audio_saw_output` runs the listening sequence under dummy SDL output with a
15-second timeout. Passing it verifies execution, not audible quality.

## Listening check

```sh
./build/sbee-saw-test
```

Lower output volume first: saw tones are brighter than triangle tones. The
roughly five-second sequence announces:

1. Left saw at 220 Hz, changing pitch to 440 Hz without retrigger.
2. Release followed by a 660 Hz retrigger while moving to center.
3. Short stop, then a fresh right-side 330 Hz note and release.
4. Nine mixed triangle/noise/saw voices, ending in silence.

Confirm correct pitch/stereo changes, clean transitions, no unexpected stuck
notes or distortion, final silence, and normal exit. This is an audible check
of bounded prototype behavior, not proof of alias-free synthesis. Also rerun
the existing noise/bank/pan/triangle/sine tools and gameplay checks. Prefer HDMI
on the Pi. The user confirmed clean desktop saw playback, correct stereo placement, and
normal exit. All 56 automated tests pass. Pi validation is recorded below.

## Pi validation

On Pi commit `f1982a9`, the user confirmed all 56 automated tests, clean saw
and earlier diagnostic tones, successful gameplay checks, and Exit to system
status 0. This validates the bounded native saw integration on desktop and Pi.
Broader pitch, live waveform transitions, and Lua controls remain deferred.

Explicit triangle/saw/pulse selection is now available separately from the
existing type-specific note-ons. See the [waveform switching contract](audio-waveform.md);
noise transitions are covered in [noise transition checks](audio-noise-switch.md).

# Standalone white-noise source

The source is a pure C mono pseudo-random noise generator, with no SDL, Lua,
allocation, or global random state. Native bank/SDL integration is described below.
It is an audio source, not a cryptographic random generator.

## Sequence and reset contract

`sb_audio_noise_t` owns one 32-bit unsigned state. Reset with a nonzero seed
restarts that sequence immediately. Seed zero maps to default seed 1; a
zero-initialized object produces the same sequence on its first rendered sample.
Reset does not fade or stop sound. There is no idle, envelope, volume, or pan
state in this component. These belong to later voice integration.

Each sample advances xorshift32 with left 13, right 17, left 5 shifts, in that
order. The upper 24 output bits `b` map to `(2*b - 16777215) / 16777216`.
These symmetric bin midpoints lie strictly between -1 and +1 and are exactly
representable on the binary32 target platforms. There is no gain or clamp.
The fixed sequence is independent of libc random functions and byte order.
Different seeds select different starting states; they do not promise
mathematically independent streams. Future bank integration must choose seeds
explicitly rather than accidentally starting every voice with seed 1.

Render replaces a mono buffer and advances once per sample, with no pitch or
sample-rate parameter. Splitting a render into several blocks preserves the
exact output and ending state. Zero samples are a no-op, including for an
uninitialized zero state, and permit null output. Null state, null output for
nonzero samples, and overflowing buffer sizes fail without modifying output
or state. The caller owns buffer capacity and synchronization; the output
must not overlap generator storage.

## Checks

```sh
ctest --test-dir build -R '^audio_noise$' --output-on-failure
```

Tests cover fixed state/sample vectors, multiple seeds, reset replay, zero seed,
zero initialization, generator isolation, block partitioning, invalid inputs,
and a million-sample statistical smoke check. Samples must be finite and bounded;
mean, mean square, sign balance, and lag-1/lag-17 correlation have broad fixed
tolerances. These deterministic checks catch obvious bias and repetition; they
do not establish a perfectly flat spectrum or replace future listening tests.

The component builds independently of the SDL/Lua host:

```sh
cmake -S . -B /tmp/sugarbee-noise-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-noise-standalone --target test_audio_noise
ctest --test-dir /tmp/sugarbee-noise-standalone -R '^audio_noise$' --output-on-failure
```

## Native bank integration

Each of the nine slots supports triangle, saw, pulse, or white noise. The native
`sb_audio_voice_noise_on` call has no frequency argument. Active noise pitch edits
retain a frequency for the next pitched transition without changing noise sound.
Type-specific note-ons reject active cross-type starts; explicit waveform
selection now supports noise transitions. ADSR, volume/pan
smoothing, mute, release, and the 10 ms short stop share the existing voice code.
Triangle note-on and earlier listening tools retain their behavior.

Every slot has a distinct fixed nonzero default noise seed, recorded in the
bank source. `sb_audio_voice_seed` configures a uint32 seed; zero restores that
slot's default (unlike the standalone source, where zero selects seed 1).
Identical explicit seeds deliberately allow identical noise sequences. Distinct
seeds avoid identical starts, but do not promise statistically independent streams.
No clock or global random state is used.

- Idle noise start resets to its configured seed, giving repeatable fresh notes.
- Active retrigger preserves sequence and attacks from the current envelope
  level over the full new attack duration, including during release/short stop.
- Seed edits affect only the next fresh noise start or transition into noise,
  never an active retrigger.
- Release, short stop, and mute continue advancing once per active frame.
  Once idle, the stream stops advancing.
- Bank reset cancels notes and restores default seeds, settings, and controls.

Mixed triangle/noise output retains fixed 1/9 headroom. Both source types are
bounded, so their summed stereo output remains within ±1 under current controls.
The SDL bank diagnostic gain remains separately approximately 0.791. Future
filters or WAV mixing still require revisiting the overall output bound.

`audio_noise_bank` checks replay, distinct defaults, pending seeds, active
retrigger timing, mute/release/stop progression, isolation, mixed rendering,
invalid/cross-type requests, reset, and nine coherent noise voices at full level.
`audio_bank_sdl` checks native noise controls and mixed callback samples.
`audio_noise_output` runs the listening tool with dummy SDL audio under a
15-second external timeout. It checks execution, not audible quality.

## Listening check

```sh
./build/sbee-noise-test
```

Lower output volume first: broadband noise may sound louder than the tone tests.
The roughly six-second sequence announces two repeatable left noise bursts,
a center noise retrigger during release followed by short stop, then left/right
noise around a center triangle and finally all nine mixed voices. Each repeated
burst restarts the same seed; exact frame timing is verified automatically,
not promised by the tool's wall-clock delays.

Confirm stereo placement, smooth envelope transitions, no unexpected stuck
sounds or distortion, final silence, and normal exit. Also rerun the earlier
bank/sine/triangle/pan tools and normal gameplay checks. Use HDMI for Pi testing.
The user confirmed clean desktop noise playback, correct stereo placement, and
normal exit. All 53 automated tests pass. Pi validation is recorded below.
Lua controls, cartridge audio
lifecycle, and tonal/SID-style noise variations remain deferred.

## Pi validation

The user reported that the Pi checklist for `557b6ba` passed with no issues:
all 53 automated tests, the noise and earlier audio listening sequences, Pong
and Save Lab gameplay/lifecycle checks, and Exit to system status 0. This
validates the current native noise integration on desktop and Pi. Lua controls,
cartridge-owned audio cleanup, and tonal noise variations remain deferred.

Explicit triangle/saw/pulse selection is now available separately from the
existing type-specific note-ons. See the [waveform switching contract](audio-waveform.md);
noise transitions are covered in [noise transition checks](audio-noise-switch.md).

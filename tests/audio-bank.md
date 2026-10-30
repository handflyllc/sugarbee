# Standalone nine-voice bank

This checkpoint adds a pure C bank, not a new listening executable or Lua API.
The SDL backend still plays its existing single-voice diagnostics.

## Contract

- Native voice numbers are 1–9. Each has independent triangle, ADSR, pitch,
  volume, pan, retrigger, and short-stop state.
- Existing prototype limits remain: 220–660 Hz, volume 0–1, pan -1–1,
  default volume 0.5 and center pan, and 10 ms live control smoothing.
- Each note captures its envelope settings. Retrigger preserves phase and
  attacks from the current level over the full new attack duration.
- Mix normalized stereo samples with fixed 1/9 gain, regardless of active
  voice count. There is no clipping, normalization by active count, or quiet
  diagnostic gain in the bank. Existing SDL diagnostics apply their own 0.05.
- Zero initialization is silent. Reset cancels all voices and restores defaults;
  it is cleanup, not an audible fade. Caller owns storage and synchronization.
- Invalid requests fail without mutation. Valid idle note-off/stop succeed
  harmlessly. Render replaces the caller's interleaved stereo buffer, uses no
  allocation, and accepts zero frames with a null output pointer.

## Automated checks

`ctest --test-dir build -R '^audio_bank$' --output-on-failure` checks silence,
invalid arguments and unchanged state, all nine slots, independent controls,
additive mixing without gain changes, note-off/stop isolation, reset defaults,
and identical results across render block sizes. Nine coherent full-level
voices are checked at left, center, and right against the analytical waveform
and ±1 output bound. This also detects accidental diagnostic attenuation.
The host audio test checks that existing SDL triangle output remains quiet.

The bank can be built without SDL or Lua host support:

```sh
cmake -S . -B /tmp/sugarbee-bank-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-bank-standalone --target test_audio_bank
ctest --test-dir /tmp/sugarbee-bank-standalone -R '^audio_bank$' --output-on-failure
```

SDL bank integration, a nine-voice listening test, and Pi performance/acceptance
checks remain for later checkpoints. This triangle-only bound does not establish
headroom for future resonant filters or mixed WAV playback.

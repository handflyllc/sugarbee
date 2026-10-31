# Standalone PolyBLEP saw

This pure C component generates normalized mono saw samples at 48 kHz and
accepts finite pitches from 220 through 660 Hz. It is not yet connected to the
bank, SDL, or Lua, and has no envelope, volume, diagnostic gain, or listening
executable. Existing playback is unchanged.

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

Bank integration, native listening, and Pi validation remain the next checkpoint.

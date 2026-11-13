# Historical native pitch checkpoint: 110–880 Hz

This records the previously accepted range. The new 27.5–4186.01 Hz runtime
checkpoint is tracked in [piano runtime validation](audio-piano-runtime.md).

Triangle, saw, and pulse accept finite frequencies from **110 through 880 Hz**,
inclusive, with fractional values. This applies to standalone oscillators,
native bank/SDL starts, active pitch edits, selected-waveform retrigger, and
retained return pitch during noise. Invalid requests fail without mutation.
The default return pitch remains 440 Hz; noise has no audible pitch control.

## Scope and pulse bounds

This checkpoint retains the direct triangle and existing PolyBLEP methods.
At 48 kHz the phase step is 110/48000 through 880/48000. The fastest pulse-width
change is 0.90/480 = 0.001875 per sample. Therefore falling-edge motion lies
between approximately 0.000416667 and 0.020208334: strictly forward throughout.

Each stored correction interval must be positive and below 0.025, half the
minimum edge spacing (0.05). The previous 0.02 cap rejected valid history after
an 880 Hz decreasing-width ramp. Both current and historical intervals now fit
within the disjoint correction regions, including abrupt pitch edits. Width
limits (0.05–0.95), 480-sample ramps, raw ±1 bounds, conditioned ±0.95 bounds,
and fixed 1/9 bank headroom are unchanged.

This is not full audio-band support. Triangle is not band-limited, PolyBLEP is
not alias-free, and phase-preserving pitch jumps are not guaranteed click-free.
No pitch glide, new voice slots, filters, Lua controls, or WAV playback are added.

## Automated checks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

- Endpoint/fractional acceptance, just-outside/nonfinite rejection, and unchanged
  state on failure, including active fades and native SDL controls.
- Direct triangle frequency crossings across 110–880 Hz and exact buffer
  partition equivalence. Coherent records at 111.328125, 439.453125, and
  873.046875 Hz compare against an independent below-Nyquist Fourier sum:
  normalized RMS error below 0.003 and fundamental amplitude within 0.001 of
  8/pi². This is a sampled regression bound, not a band-limiting guarantee.
- Existing saw/pulse spectral checks extended near both new endpoints, retaining
  the 10 dB unwanted-power reduction and fundamental-preservation thresholds.
- Pulse full-width ramps, retargets and endpoint pitch jumps; sample-by-sample
  rendering verifies history remains valid, including intervals above 0.02.
  Moving-width reference checks extend to 117.1875 and 867.1875 Hz, retaining
  the existing reference-error threshold. Settled DC and live conditioning
  checks cover both expanded ends.
- All six pitched transition directions at 110 and 880 Hz, noise return pitch,
  selected retrigger deadlines, bounded nine-voice output, and SDL callback gain.
- The listening executable also runs with SDL's dummy driver under CTest.
  That verifies execution and cleanup, not audible quality.

## Desktop listening, then Pi validation

```bash
./build/sbee-pitch-test
```

The approximately 18-second sequence prints each section:

1. Triangle, saw, then pulse: separate 110, 220, 440, and 880 Hz notes with gaps.
   Confirm each octave rises and each waveform remains recognizable.
2. Pulse: live 110 → 880 → 110 Hz changes with width sweeps. Listen for unwanted
   dropouts, persistent buzz after a transition, or stuck notes. Changing duty
   cycle intentionally changes timbre and loudness.
3. 880 Hz triangle → saw → pulse → noise → 110 Hz triangle. Confirm the return
   is the lower tone and the program exits normally.

Use the existing waveform/noise-switch diagnostics for comparison if needed.
After desktop acceptance and an authorized commit/push, build and run all tests
on Pi, repeat this listening check via HDMI, then check Pong/Pac-Man and Exit to
system status 0. Record user observations against the actual tested commit.

Status (2026-11-01): Debug build and all **67/67 tests passed** on the development
machine. Triangle reference RMS errors were 0.00007421, 0.00058972, and
0.00157747 at the three listed frequencies. Moving-pulse selected-bin reference
error reduction at the added low/high records was 7.60/8.87 dB. These are
regression measurements, not listening results. The user reported that the desktop
`sbee-pitch-test` performed as expected on 2026-11-01. This report applies to the
uncommitted checkpoint on `feature/audio-pitch-range`, based on `1827c50`.
The checkpoint was subsequently committed as `ff61ef6`.

Pi report (2026-11-02), for the requested `ff61ef6` validation: the user confirmed
that audio checks, Pong/Pac-Man labs, and Exit to system checks passed.
The user also confirmed the Pi built successfully and the test run reported
zero failures. Pi validation is complete for this checkpoint. These hardware
results are user-reported; they were not independently run in this session.

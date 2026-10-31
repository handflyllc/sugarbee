# Standalone fixed-width PolyBLEP pulse

The pure C mono oscillator runs at 48 kHz, with finite pitch 220–660 Hz and
pulse width 0.05–0.95. Width is the high fraction of each cycle. The default
width constant is 0.5 (square wave). Width is supplied to reset and held fixed;
there is no live width setter, bank/SDL integration, or listening executable.

## State and signal contract

Reset starts at the rising edge, phase zero, where the corrected first sample
is zero. Zero-initialized storage is unconfigured and requires reset. Pitch edits
preserve phase and width. Render outputs the current phase then advances once
per sample. Buffer partitioning does not change output or ending phase.
Invalid pitch, width, state, pointers, or overflowing counts fail unchanged.
Zero samples permit null output but require configured state. Callers own
synchronization, buffer capacity, and non-overlapping storage. Reset is not a fade.

The raw levels are +1 and -1. PolyBLEP adds local quadratic corrections at both
rising and falling edges. Output remains within ±1 with no clamp, attenuation,
DC removal, or loudness normalization. With the current pitch/width ranges the
correction regions do not overlap; widening either range needs a new review.

The expected cycle average is `2*width - 1`:

| Width | Expected DC offset |
| --- | --- |
| 0.05 | -0.90 |
| 0.25 | -0.50 |
| 0.50 | 0.00 |
| 0.75 | +0.50 |
| 0.95 | +0.90 |

The nonzero average is intentional for this standalone conventional pulse.
It consumes signal headroom without representing an audible pitch. DC handling
must be reviewed before bank integration, including envelope/width transitions
and any filtering. Do not assume a future filter or the output device removes it.

Method reference: [PolyBLEP pulse overview](https://www.orastron.com/algorithms/bw_osc_pulse).
No external runtime library is introduced. Aliasing is suppressed, not eliminated.

## Automated checks

```sh
ctest --test-dir build -R '^audio_pulse$' -V
```

Checks cover known edge/plateau samples, correction interiors, reset replay,
width and pitch boundaries, finite bounded output, invalid requests, pitch-only
phase preservation, and render partition independence. A phase grid at maximum
pitch covers widths including both narrow-duty extremes.

Coherent 8192-sample records at 228.515625, 439.453125, and 650.390625 Hz test
widths 0.05, 0.25, 0.5, 0.75, and 0.95. Their mean must be within 1e-5 of the
expected DC value. A separate AC metric removes DC and intended harmonic-bin
power, then compares remaining power with an uncorrected pulse. At least 10 dB
reduction is required while retaining fundamental amplitude within 1% of the
raw waveform and near `4/pi * sin(pi*width)`. Measured desktop reductions were
15.53–18.68 dB. These measurements exclude aliases landing in intended harmonic
bins and do not prove quality at every frequency, width, or during modulation.

Build independently of the SDL/Lua host:

```sh
cmake -S . -B /tmp/sugarbee-pulse-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-pulse-standalone --target test_audio_pulse
ctest --test-dir /tmp/sugarbee-pulse-standalone -R '^audio_pulse$' --output-on-failure
```

Live width changes, DC handling for integration, native bank/SDL controls,
listening, and Pi validation remain later checkpoints. Lua remains deferred.

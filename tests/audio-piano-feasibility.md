# A0–C8 feasibility: isolated oscillator experiment

Historical investigation checkpoint: `de04c52`. Runtime integration is now
tracked in [runtime validation](audio-piano-runtime.md). Reproduce the isolated
source-patching scripts from `de04c52`; they intentionally refuse the integrated
source tree. Statements below about unchanged runtime describe that checkpoint.

2026-11-13, source baseline `b4b6bcf`. Investigation only; production still
accepts 110–880 Hz. Nine voices, mixing gains and 48 kHz remain unchanged.
This is preliminary oscillator evidence, not completed engine/Pi acceptance.

## Reproduce

From the repository root, with a C99 `cc` compiler and Python 3 with NumPy:

```sh
python3 tests/experiments/pitch_feasibility.py
```

The script builds temporary shared libraries from current pulse/saw sources;
no production file is rewritten and no CMake target changes. Compiler warnings
are errors. Dependencies are offline experiment tools, not engine dependencies.
Triangle is a mathematical model of the current direct waveform; its envelope,
controls, bank and SDL integration are not exercised.

## Findings and candidate

Merely widening frequency guards is insufficient. Pulse state validation
requires prior intervals below 0.025 cycles and positive falling-edge motion.
C8 has a 0.0872085-cycle sample step. At A0, an increasing 0.05→0.95 width
ramp has falling motion 27.5/48000 − 0.9/480 = −0.00130208 cycles/sample.
The existing renderer can reject these states, including after a successful
initial render. Its non-overlap assumptions also cease to apply at narrow widths.

The temporary candidate widens guards and independently sums past/future edge
residuals for either motion direction, including zero motion. It retains the
480-frame width ramp and 0.05–0.95 width limits. Experimental history bounds
(-0.002 to 0.1 for falling motion) are deliberately provisional: production
validation needs derived constants and invalid-state tests.

Desktop results (exit 0):

- Existing native live-width regression passes unchanged against the candidate,
  including its independent 32× reference comparisons.
- Sampled old-band width trajectories at 110, 110.125, 220, 440, 660, 879.75
  and 880 Hz are bit-identical to baseline, raw and conditioned.
- Sampled width ramps, A0/C8 pitch jumps and 37-frame versus event-sized buffer
  partitions pass at 27.5, 60, 90, 110, 880, 1760 and C8 Hz. Raw peak is 1;
  conditioned peak is 0.94999999. These are measured cases, not a universal proof.
- Independent periodic 64× PWM references at coherent carriers 46.875 through
  4171.875 Hz give 5.81–8.91 dB less total positive-bin reference error than
  naive PWM. Largest measured carrier-coefficient error is 0.004261.
  DC and Nyquist bins are excluded; no claim of complete DC or alias validation.

## Static reference comparison

8192 samples against a Fourier waveform truncated strictly below Nyquist.
Errors are RMS in raw full-scale units, including passband attenuation and
aliasing together; **not alias-only energy**, perceptual scores, or relative dB.
Pulse results below use 50% width; script also checks 5% and 95%.

| Frequency | Triangle | Saw | Pulse |
| --- | ---: | ---: | ---: |
| A0, 27.5 Hz | 0.00000910 | 0.00646289 | 0.00907775 |
| 110 Hz | 0.00007362 | 0.01293402 | 0.01811259 |
| 880 Hz | 0.00160842 | 0.03660783 | 0.05237102 |
| 1760 Hz | 0.00454315 | 0.05131176 | 0.07390978 |
| 3520 Hz | 0.01537613 | 0.07119637 | 0.09752424 |
| C8, 4186.009045 Hz | 0.01552450 | 0.07767757 | 0.11426209 |

The candidate keeps sampled static raw peaks within ±1, including narrow
pulses. Triangle's upper-register deviation deserves listening; this measurement
alone does not justify replacing its synthesis. PolyBLEP saw/pulse also lose
upper harmonics relative to the ideal reference; do not equate all error with
unwanted audible aliases.

## Recommendation and remaining gates

Proceed with this pulse candidate as a feasibility prototype, not a runtime
patch yet. Keep existing triangle/saw algorithms for the next comparison, and
prepare matched-level listening/reference captures across the upper register.
Do not add oversampling or a new waveform without demonstrated need.

For the next candidate evaluation, retain bit-identical sampled old-band pulse
output, exact partition invariance, raw ±1 and conditioned ±0.95 bounds (float
rounding tolerance 1e-7). Use at least 3 dB moving-reference improvement and
carrier complex-coefficient error below 0.01 as provisional regression gates,
following the existing test's intent. These do not establish listening quality.
Add targeted signed/zero-motion crossing and retarget tests, broader phase and
width sampling, and oversampling-convergence checks before accepting the method.

Still open: isolated alias/passband measurements and listening criteria; low-note
DC/filter behavior; full bank transitions/lifecycle; allocation-free nine-voice
plus WAV callback cost on Pi, measured against its actual buffer deadline.
No desktop timing substitute is accepted for Pi performance. Endpoint policy
was subsequently agreed on 2026-11-13: finite 27.5–4186.01 Hz inclusive,
played unchanged, with no clamping, snapping or extra tolerance. Apply the same
bounds to all native/Lua/music validation. Existing assets remain compatible;
newly out-of-range notes require the updated engine, without a format change.
The preserved experiments use exact equal-tempered C8; they are historical
evidence, not a test of the slightly higher rounded ceiling. Implementation
boundary tests must cover both exact C8 and 4186.01, adjacent out-of-range
values, nonfinite inputs and refusal without state mutation.

Only after these decisions should implementation unify native/Lua/asset pitch
validation and update consumers' dependency guidance. Existing cartridges and
asset format remain unchanged; older engines reject new out-of-range notes.
SugarBeats changes, percussion, voice-count changes and sample-rate changes are
outside this checkpoint.

## Upper-register listening checkpoint — desktop accepted

Generate the fixtures with:

```sh
python3 tests/experiments/pitch_listening.py
```

Five 20.3-second, mono, 48 kHz/16-bit WAVs are written to ignored
`build/pitch-listening/`: triangle, saw, pulse at 50%, 5%, and 95% width.
`manifest.json` records source/output hashes, precise frequencies, timestamps,
and individual normalization gains. The generator checks finite samples,
headroom, file format/frame counts, and decoded steady RMS within half a PCM
quantization step of the target. All five passed on desktop; independent
ffprobe inspection confirmed the final file's encoding and duration.

Each note is played **candidate first, Fourier reference second**. Each tone
lasts 1.2 seconds, separated by 0.3 seconds within a pair and 0.6 seconds between
pairs, with an initial 0.5-second silence. The reference contains only harmonics
strictly below Nyquist. Candidate saw/pulse use the temporary native C library;
triangle uses the current oscillator formula. These are isolated oscillator
fixtures, not engine captures or full-bank/lifecycle tests.

| Note | Candidate starts | Reference starts |
| --- | ---: | ---: |
| A5 (880 Hz) | 0.5 s | 2.0 s |
| C6 (1046.50 Hz) | 3.8 s | 5.3 s |
| A6 (1760 Hz) | 7.1 s | 8.6 s |
| C7 (2093.00 Hz) | 10.4 s | 11.9 s |
| A7 (3520 Hz) | 13.7 s | 15.2 s |
| C8 (4186.01 Hz) | 17.0 s | 18.5 s |

Audition-only RMS normalization sets each tone's steady RMS to 0.08 (about
−22 dBFS), with 20 ms raised-cosine fades; all encoded peaks remain below 0.5.
Pulse uses the existing conditioned-output convention. No per-tone normalization
is proposed for production. Matching RMS is not perceptual loudness matching,
and conceals absolute gain differences; fixed-gain comparisons remain a separate
check if needed. No filters, stereo positioning, music or WAV mixing are applied.

Listen at a comfortable volume, starting with triangle, then saw and 50% pulse;
use the narrow pulses last. Keep playback EQ/normalization disabled if practical.
Record the waveform and earliest note where the first tone has an objectionable
extra whistle, rough buzz, beating, or apparently wrong pitch compared with the
second. Also record whether the difference is merely brightness/timbre, whether
both tones are uncomfortable, or whether no meaningful difference is apparent.
Do not treat every audible difference as aliasing, or either waveform as an
exact sound the console must adopt. The 5%/95% files should sound broadly similar
in steady tone but need not be sample-identical due to phase.

User listening accepted on 2026-11-13 for these generated desktop fixtures:
triangle and saw sounded fine. User explicitly reported no artifacts in either
pulse extreme; 5% sounded saw-like, 95% harsher, and 50% was the preferred,
less-harsh tone. This supports retaining the width range and existing default;
it is not Pi acceptance or proof that every pitch/transition is artifact-free.
No runtime changes or commits were made for this checkpoint.

## Pulse edge and low-note checks — 2026-11-13

Run `python3 tests/experiments/pitch_edge_checks.py` (same offline dependencies).
This compiles temporary candidate oscillators and the unchanged native filter;
production files remain untouched. Desktop exit 0:

- 4095 combinations of phase, pitch, width, prior edge motion and future width
  targets matched independent numerical triangular-kernel integration within
  0.00006583 raw amplitude (gate 0.0003). Included edge coincidences, backward
  motion, reversals and high-frequency narrow pulses. Three additional exactly
  stationary falling-edge cases passed. This validates the correction's local
  kernel behavior, not ideal alias-free synthesis.
- 500 seeded pitch/width retargets stayed finite and bit-identical across buffer
  partitions. Largest conditioned magnitude was 0.94842947 (gate 0.95 + 1e-7).
- At 27.5, 55 and 110 Hz, triangle (formula), native saw and conditioned pulse
  at widths 0.05/0.5/0.95 were checked through the native filter. Cutoffs were
  20, 27.5, 2000 and 20000 Hz. After two seconds of settling, two-second windows
  contained integer cycles. Largest absolute mean was about 1.3e-9 (gate 2e-6).
- Measured complex fundamental filter response matched the discrete one-pole
  transfer function within 4.28e-12 (gate 1e-7). Outputs were finite and bounded
  by input peak; bypass was sample-exact. A constant-input control confirms the
  low-pass passes DC: oscillator conditioning, not this filter, removes pulse DC.

Reference development exposed cancellation at nearly stationary edges. The
integration reference now evaluates rising/falling crossing coordinates
separately rather than subtracting nearly equal moving phase/width values.
The candidate did not change. Quadrature is numerical and has a stated tolerance.
These are sampled local/steady-state checks; live filter-control ramps, complete
bank lifecycle, a universal bound proof and Pi callback performance remain open.
The next checkpoint is preparing/measuring the nine-voice plus WAV callback
workload on Pi before authorizing production integration. Keep voice count,
mixing gains, sample rate and the 480-frame width ramp unchanged.

## Pi benchmark prepared

See [render-cost benchmark](audio-pitch-benchmark.md) for the isolated source
bundle, desktop evidence, provisional headroom targets and Pi commands. The
benchmark uses the shared experimental patch helper; production remains
unchanged. Returned Pi 4 measurements pass the offline screening targets: worst
candidate-wide block 0.8100 ms against a nominal 10.6667 ms deadline, with zero
misses and timed heap calls. See the linked report for source verification and
limits; live SDL/gameplay validation remains pending.

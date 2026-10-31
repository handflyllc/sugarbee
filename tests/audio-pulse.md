# PolyBLEP pulse

The pure C mono oscillator runs at 48 kHz, with finite pitch 220–660 Hz and
pulse width 0.05–0.95. Width is the high fraction of each cycle. The default
width constant is 0.5 (square wave). Reset supplies the initial width. Live edits ramp over 480 samples (10 ms);
native bank/SDL integration and a listening executable are described below.

## Raw state and signal contract

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
It consumes signal headroom without representing an audible pitch. The explicit
conditioned renderer below removes the settled expected offset. Do not assume
a future filter or the output device removes it.

Method reference: [PolyBLEP pulse overview](https://www.orastron.com/algorithms/bw_osc_pulse).
No external runtime library is introduced. Aliasing is suppressed, not eliminated.

## Automated checks

```sh
ctest --test-dir build -R '^audio_pulse' -V
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
cmake --build /tmp/sugarbee-pulse-standalone --target test_audio_pulse test_audio_pulse_width test_audio_pulse_dc test_audio_pulse_bank
ctest --test-dir /tmp/sugarbee-pulse-standalone -R '^audio_pulse' --output-on-failure
```

Native bank/SDL controls are now implemented. Desktop and Pi validation passed;
Lua remains deferred.

## Live width checkpoint

`sb_audio_pulse_width()` accepts finite targets 0.05–0.95. A different target
ramps linearly from the current width over a full 480 rendered samples. Repeating
the same target does not restart its deadline. Width edits preserve phase and
pitch; reset applies width immediately and cancels the ramp. The standalone
oscillator has no note/idle state; bank controls store idle edits and apply them
immediately at the next pulse start. Rendering zero samples does not advance the ramp.

The falling-edge correction accounts for relative motion: phase increment minus
width increment. Corrections on either side use the corresponding previous or
next interval. Under the current pitch, width, and ramp limits, that motion stays
positive and the edge correction regions remain disjoint. This is not a general
implementation of arbitrary audio-rate pulse-width modulation.

`audio_pulse_width` covers exact timing, retargeting, repeated targets, reset,
phase preservation, chunk independence, invalid edits, known moving-edge samples,
and bounds during extreme reversals. A separate spectral check compares selected
baseband DFT bins with an independently generated 32x-rate raw PWM reference.
The reference includes legitimate modulation sidebands. At 257.8125, 445.3125,
and 632.8125 Hz, corrected reference error was 7.41, 6.62, and 7.12 dB lower
than raw output; the test requires at least 3 dB and checks carrier preservation.
Finite oversampling and selected bins make this a regression metric, not proof
of alias-free output or every modulation trajectory.

## Explicit DC conditioning

`sb_audio_pulse_render()` retains raw output. The separate
`sb_audio_pulse_render_conditioned()` applies
`0.5 * (raw - (2*current_smoothed_width - 1))`. This subtracts the
settled expected DC value with fixed gain to retain bounded output. It halves
square-wave amplitude (about 6 dB); there is no automatic loudness normalization.
It does not promise zero average over short transition windows. Native bank
integration uses this conditioned renderer.

Conditioning uses the width for the sample being rendered, before advancing the
ramp, rather than the target or next width. Current raw and width bounds imply
conditioned output within ±0.95, without clipping. The raw square's ±1 plateaus
become ±0.5; narrow pulses retain their natural level differences. Reset has no
fade: its first conditioned sample is `0.5-width`, and may be nonzero.

Both render entry points advance the same oscillator state identically. Use
copies to compare raw and conditioned versions of the same samples. There is no
DC-blocking filter state, allocation, envelope, or live output-mode crossfade.
Do not alternate render modes expecting a click-free transition.

`audio_pulse_dc` checks coherent settled means across five widths and three
pitches, raw-output preservation, square amplitude, same-sample conditioning
during full ramps and reversals, bounds, render partition independence, reset
replay, and invalid/zero-length requests. It does not require zero average over
short transition windows or claim audible validation.

## Native bank and SDL integration

`sb_audio_bank_pulse_on` / `sb_audio_voice_pulse_on` start or retrigger a pulse
in an explicit slot 1–9, at 220–660 Hz. Idle starts reset phase to zero and apply
the stored width (default 0.5). Active pulse retriggers preserve phase and width
ramp, including release/short-stop retriggers. Starting a different waveform in
an active slot is rejected without mutation; idle slots may change type.

`sb_audio_bank_pulse_width` / `sb_audio_voice_pulse_width` accept finite 0.05–0.95.
Active pulses ramp for 480 samples; repeated targets do not restart the ramp.
For idle slots or other waveforms, width is retained for the next pulse start.
A note that ends during a ramp starts next time at its stored target. Reset
restores default width. Pitch edits preserve phase and width state.

Conditioning runs before the shared ADSR, volume, and equal-power pan. Mute
still advances oscillator, width, and envelope. The existing 10 ms short stop
and captured ADSR semantics apply. Nine voices retain fixed 1/9 mixer headroom;
the SDL diagnostic gain remains separately 0.790569415. No pulse-specific
loudness compensation, allocation, callback logging, or Lua controls were added.
SDL controls lock the device; rendering remains in the existing callback path.

`audio_pulse_bank` checks all nine slots against the conditioned oscillator,
retrigger/pitch/width preservation, idle target retention, type/argument rejection,
muted progression, release/stop, narrow-pulse headroom, mixed-source partitioning,
slot independence, and reset. `audio_bank_sdl` checks real wrapper/callback routing,
diagnostic gain, legacy-test exclusion, invalid controls, and active teardown.
`audio_pulse_output` runs the listening program with a dummy device and a timeout;
it verifies completion, not audible quality.

Run on the intended audio output:

```sh
./build/sbee-pulse-test
```

Listen for the left pulse changing pitch, the centered width sweep (square to
5%, then 95%), release/retrigger, a fresh right pulse, and a nine-voice mix of
triangle/noise/saw/pulse. Width changes intentionally alter timbre and loudness.
Check for unexpected clicks/distortion, correct placement, and normal exit.
The user confirmed all desktop listening checks: clean pitch/width/retrigger
playback, correct stereo placement, clean mixed voices, and normal exit. All 61
automated tests passed. On Pi commit `a397e26`, the user confirmed all 61 tests,
clean pulse/earlier tones, passing gameplay checks, and Exit to system status 0.

On Raspberry Pi OS Lite 64-bit at `51cbc8b`, the user reported all 61 tests
passed, clean sine and pulse audio, correct stereo placement, and normal test
exits. Pong and Pac-Man played without noticeable lag. Save Lab persisted
through relaunch/repacking and Exit to system returned 0.

Responsiveness follow-up: keyboard and controller felt delayed in the desktop
session but responsive in the TTY. The delay was not reproduced on Lite. The
exact desktop-associated cause remains undiagnosed; these are subjective checks,
not latency measurements or evidence that audio caused the delay.

Explicit triangle/saw/pulse selection is now available separately from the
existing type-specific note-ons. See the [waveform switching contract](audio-waveform.md);
noise transitions remain deferred.

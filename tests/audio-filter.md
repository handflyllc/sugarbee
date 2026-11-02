# Native per-voice low-pass filter

Each of the nine bank voices has one non-resonant, one-pole low-pass filter.
The filter is **bypassed by default**, with a retained cutoff of **2000 Hz**.
Cutoff accepts finite **20–20,000 Hz** values, including fractional values.
This cutoff range is separate from the oscillator pitch range of 110–880 Hz.

## Controls and lifecycle

- `sb_audio_voice_filter_cutoff(audio, voice, cutoff)` sets cutoff without
  enabling the filter. `sb_audio_voice_filter_bypass(audio, voice, bypass)`
  selects original sound (`true`) or low-pass output (`false`).
- Pure C counterparts are `sb_audio_bank_filter_cutoff` and
  `sb_audio_bank_filter_bypass`. Invalid pointers, slots, and cutoff values fail
  without mutation. Voice slots are 1–9. SDL wrappers lock the device and are
  for the host control thread; there is no Lua API yet.
- Idle edits apply immediately and do not start a note. Active edits use
  480-sample (10 ms) linear ramps of the **filter coefficient** and **dry/wet
  blend**, not a linear ramp of the cutoff in Hz. Repeating a target preserves
  the deadline; a new target begins from the current value.
- Render uses the current controls, then advances them. A change uses the old
  value for sample 0, reaches the target after 480 samples, and uses the exact
  target on the following sample.
- Filtering follows waveform crossfading and precedes ADSR, volume, pan, and
  fixed 1/9 mixing headroom. There is one filter history per voice, not one per
  oscillator. Active retriggers and waveform switches preserve that history.
- Fresh idle starts clear history to zero and settle unfinished control ramps
  to retained targets. All pitched/noise starts obey this rule. Whole-bank reset
  restores defaults. Note-off and short stop retain their existing envelope
  timing; no filter tail prolongs a voice after it becomes idle.
- Muted or bypassed active voices still advance filter history and ramps.
  Idle voices do not process samples. Exact bypass returns the input unchanged.
- Legacy standalone sine/triangle diagnostics remain unfiltered.

## Method and bounds

At 48 kHz, the filter recurrence is `y += a * (input - y)`. For cutoff `f`, set
`k = 1 - cos(2*pi*f/48000)` and `a = sqrt(k*k + 2*k) - k`. This is the positive
solution giving a settled magnitude of `1/sqrt(2)` (approximately -3 dB) at
cutoff. Coefficients are computed on control/start operations; the render path
has no allocation, logging, Lua calls, or transcendental functions.

For supported cutoffs, `0 < a < 1`. Linear interpolation preserves that bound,
so each filtered sample is a weighted average of input and prior history.
Starting from zero, bounded ±1 source input stays within ±1. The bypass blend
is also a weighted average, preserving the existing bank bound and fixed gain.
There is no limiter, resonance, gain boost, or loudness compensation. The
one-pole response is gentle; near Nyquist it does not follow the analog
6 dB/octave approximation exactly. Filtering does not remove aliasing already
introduced during oscillator sampling.

## Automated verification

`audio_filter` checks exact bypass, default settings, invalid-request atomicity,
ramp deadlines/retargeting, measured cutoff response, unity DC gain, increasing
attenuation with frequency, and bounded output under rapid edits. Bank checks
cover placement after waveform blending, active retrigger, fresh-start cleanup,
slot independence, muted advancement, stop/reset, nine-voice headroom, and
identical output across buffer partitions. Existing regressions verify that
default bypass preserves previous audio behavior. `audio_bank_sdl` exercises
controls and callback output, and `audio_filter_output` runs the listening tool
with a dummy device (execution coverage, not a listening result).

## Listening and Pi checklist

```bash
./build/sbee-filter-test
```

The approximately 15-second comparison plays a steady 440 Hz saw and then noise:
original → cutoff 1200 Hz → cutoff 120 Hz → gradual rise to 12000 Hz → original.
Confirm the softened/muffled sections lose brightness and the return to bypass
restores it. Lower cutoffs can make the sound quieter, especially the low saw
setting. Listen for unexpected dropouts, sharp clicks at control changes, stuck
notes, or failure to exit. No SID or synthesizer familiarity is required.

After desktop listening acceptance and commit/push, repeat the full build/test
suite and listening comparison on Pi, then check Pong/Pac-Man, pause/resume,
and Exit to system status 0. Keep Pi edits local; it remains receive-only.

Status (2026-11-02): Debug build succeeded without compiler warnings; all
**69/69 tests passed**. The user accepted desktop listening on 2026-11-02
("The sound is perfect") for the uncommitted checkpoint based on `afad217`.
Pi validation is pending. No resonance, additional filter modes, automatic
filter envelopes, Lua binding, WAV support, or voice-count changes are included.

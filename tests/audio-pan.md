# Native triangle volume and pan

With low system volume and stereo speakers/headphones, run:

```sh
cmake --build build
./build/sbee-pan-test
```

The labeled sequence holds a 440 Hz note while moving left, center, right, then
center again. It then becomes quieter, mutes, restores volume, and releases to
silence. Check channel direction, centered placement, smooth changes without
obvious clicks, silence during mute, and normal exit. Perceived loudness depends
on your speakers and listening position. See [Pi output troubleshooting](audio-tone.md#pi-troubleshooting-no-audible-tone).
All 47 automated tests pass. The user confirmed correct desktop pan direction,
mostly smooth volume changes, silent mute, and normal exit. This records the
reported result without claiming perfectly smooth transitions. On Pi commit
`aa4f408`, the user confirmed all 47 tests, pan/volume checks, earlier tone and
gameplay checks, and Exit to system status 0 passed.

## Contract

- Volume accepts finite 0–1; default 0.5. Pan accepts finite -1–1; default zero.
  Invalid requests change nothing. Controls are native-only and use the existing
  SDL device lock; normal host startup remains silent.
- The fixed 0.05 testing attenuation remains separate from volume. Center uses
  equal channel gains of sqrt(0.5), so the triangle is quieter than the earlier
  full-amplitude-in-both-channels diagnostic. The sine diagnostic is unchanged.
- Equal-power gains are `cos((pan+1)*pi/4)` and `sin((pan+1)*pi/4)`.
  Exact endpoints silence the opposite channel; center uses identical gains.
  Squared channel gains sum to one, including intermediate ramp positions.
- Active parameter changes ramp linearly from the current value to the target
  over 480 frames (10 ms at 48 kHz). Pan gains are computed from each interpolated
  pan value; interpolating channel gains directly would not preserve equal power.
  The current value is used for a sample, then advanced for the next sample.
- A changed target during a ramp starts a fresh 480-frame ramp from the current
  value. Repeating the existing target does not restart its ramp. Volume and pan
  ramp independently and do not change oscillator phase or ADSR progress.
- Idle changes apply immediately, since there is no sounding transition to smooth.
  An idle restart uses the retained targets. Active retrigger preserves control
  ramps. Muting does not stop the voice or freeze attack, sustain, or release.

The listening executable waits at most two seconds for release, then supplies
250 ms of silence before teardown. Successful completion returns 0, failure 1;
this does not guarantee hardware drain or bound a stalled driver.

## Automated coverage

`audio_pan_samples` checks endpoint silence, center equality, intermediate gains,
equal-power energy, exact smoothing duration, simultaneous ramps, mid-ramp
retargeting, invalid/nonfinite requests without mutation, muted oscillator and
ADSR progression, buffer partition independence, and control retention through
retrigger/restart. It has a five-second external timeout.
`audio_pan_output` runs the real listening executable using dummy audio with a
ten-second timeout. Existing triangle and host tests remain in the full suite.
These tests establish generated samples and control behavior, not audibility.

The voice still uses a direct, non-band-limited triangle at 110–880 Hz. Multiple
voices, Lua controls, filters, other waveforms, and WAV playback remain deferred.

## Pi output observation

The user ran the validation with both HDMI and A/V output. HDMI sounded higher
quality; A/V sounded notably degraded. The cause was not determined. Possible
connection, wiring, or interference explanations are hypotheses, not diagnoses.
HDMI is the preferred output for future Pi audio validation. A/V audio quality
is not an acceptance requirement for the current milestone, and this observation
does not add A/V troubleshooting to the implementation scope.

The standalone bank checkpoint normalizes the underlying triangle renderer.
The existing single-voice SDL diagnostic path applies the separate 0.05 gain,
retaining this listening tool's quiet output. The new nine-voice bank is not
connected to this tool; see [bank tests](audio-bank.md).

A later comparison using the same wired headphones on the development computer
produced clean audio; speakers there also sounded clean. This points toward the
Pi A/V output path as the source of the earlier degradation, without identifying
its exact cause. HDMI remains the preferred Pi output.

# Single native triangle prototype

Start with low speaker/headphone volume, then run from the repository root:

```sh
cmake --build build
./build/sbee-triangle-test
```

Expect a centered triangle tone at 220 Hz, then 440 Hz, then 660 Hz (about
700 ms at each pitch), followed by a short fade and silence. Check for correct
output, steady tones, no obvious clicks at pitch changes, and clean exit. A
triangle sounds different from the diagnostic sine; the pitch changes are
instant steps, not glides. See [Pi output troubleshooting](audio-tone.md#pi-troubleshooting-no-audible-tone)
if no sound is heard. The user confirmed clean desktop triangle tones, no obvious clicks at pitch
changes, and normal exit. All 44 automated tests pass. Pi validation remains
pending.

This is a native-only prototype, not the nine-voice synth API. It has fixed
peak amplitude 0.05, identical stereo channels, and linear ten-ms start/stop
fades. The temporary accepted pitch range is 220–660 Hz, including fractional
values. Nonfinite/out-of-range pitches are rejected without changes. This range
is a deliberately narrow testing scope, not a guarantee of alias-free output:
the direct triangle formula is not band-limited. Full-range aliasing control
must be addressed before publishing the synthesis API.

Start resets an idle voice to a zero crossing and refuses an already active
voice. Pitch changes preserve phase and fade progress, including during stopping.
Stop begins a fade from the current level; repeated stop calls do not restart
it. Silent stop is harmless. The final fade completes after 480 rendered frames,
then output is exactly silent. These controls are temporary and do not establish
future note-on retrigger or ADSR behavior.

SDL device locking serializes native requests with callback advancement.
The callback allocates nothing and calls neither Lua nor logging functions.
The diagnostic sine and triangle refuse to start while the other is active;
this checkpoint does not introduce mixing. Normal SugarBee never requests either.

The listening executable waits at most two seconds for stop completion, supplies
250 ms of silence, then closes. It returns 0 on completion and 1 on unavailable
audio, rejected controls, or timeout. This is not a hardware-drain guarantee;
driver-level blocking is outside the application timeout. A completion message
does not prove audibility.

## Automated checks

```sh
ctest --test-dir build -R '^audio_' --output-on-failure
```

`audio_triangle_samples` verifies idle silence, frequency at the three test
pitches, bounded finite output, equal channels, start fade, phase preservation
across pitch changes, rejection without mutation, stopping during attack,
repeated-stop behavior, exact stop duration, restart, and chunk-independent output.
It has a five-second timeout. `audio_triangle_output` exercises the actual native
tool with dummy output and a ten-second timeout. Host tests verify mutual
exclusion with the sine diagnostic, native control completion, and unavailable
behavior. No automated test plays through a real device.

Lua controls, nine voices, configurable envelopes, filtering, pan/volume controls,
WAV playback, and full-range synthesis remain later checkpoints.

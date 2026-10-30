# Native triangle with ADSR

Start with low speaker/headphone volume, then run from the repository root:

```sh
cmake --build build
./build/sbee-triangle-test
```

The labeled sequence demonstrates:

1. Attack/decay/sustain at 220 Hz, then a pitch change to 440 Hz without restarting
   the envelope.
2. Note-off using the configured release, followed by silence.
3. Another note at 440 Hz, release, then retrigger at 660 Hz from its current level.
4. A note with ten-second release, ended by the separate ten-ms stop.

Expect gentle level changes, a smooth rise on retrigger rather than an abrupt
volume reset, and a quick final stop followed by normal exit. Pitch changes are
steps, not glides. See [Pi output troubleshooting](audio-tone.md#pi-troubleshooting-no-audible-tone)
if the intended output is silent. The user confirmed this updated sequence sounds clean on desktop, with smooth
transitions and normal exit. On Pi commit `710b738`, the user confirmed all 45
tests passed, clean ADSR/retrigger and sine output, normal gameplay checks, and
Exit to system status 0.

## Native contract

This remains one native voice, 220–660 Hz, fixed attenuation 0.05, with native volume/pan controls (default volume
0.5 and equal-power center). See [volume/pan](audio-pan.md). The direct triangle is not band-limited; full-range aliasing control is
still deferred. It is not the nine-voice synth or a Lua API.

Complete ADSR configuration validates all fields before changing anything.
Defaults are attack 0.01 s, decay 0.10 s, sustain 0.7, release 0.15 s. Each
start/retrigger captures the current settings, including its future release.
Configuration edits alone neither retrigger nor alter the current envelope.

Idle start begins at a waveform zero crossing with level zero (unless immediate
stages change that level). Active start retriggers: it preserves phase, sets the
new pitch, and attacks from the current level toward one over the full new attack
duration. At level one it holds there for that duration before decay. This also
applies during release or a short stop; a new note cancels that stop. The previous
note is replaced rather than overlapped.

Pitch-only control preserves phase and envelope progress. Note-off releases from
the current level using captured settings. Repeating note-off during release is
harmless. Stop overrides release with a ten-ms linear fade from the current level;
repeated stop or note-off during that fade cannot restart or lengthen it.
Silent note-off/stop is harmless. Configuration is retained after either finishes.

For each sample the renderer uses the current envelope level, then advances it
one frame for the next sample. Controls operate at that next-sample boundary.
Zero-duration stages remain immediate and may cause clicks; no hidden smoothing
changes their meaning. Nonzero release/stop completion produces exact silence.

SDL device locking protects native requests. The callback does no allocation,
logging, or Lua access. The sine diagnostic and triangle remain mutually
exclusive. Normal SugarBee requests neither and stays silent.

## Checks and boundaries

```sh
ctest --test-dir build -R '^audio_' --output-on-failure
```

Tests cover waveform frequency, stereo equality, bounds and buffer-independent
rendering, ADSR values/stages, captured settings, atomic invalid-request rejection,
phase-preserving pitch/retrigger, full-duration attack from current level, retrigger
at level one and during release/stop, immediate release, and short-stop duration.
Host tests exercise controls through the SDL backend. The dummy-driver listening
executable has a ten-second external timeout; it does not establish audibility.

Each tool wait for release/stop is capped at two seconds, followed at the end by
250 ms of silence. Exit status is 0 on completion, 1 on request failure/unavailable
audio/timeout. These waits do not bound a stalled driver or guarantee hardware
drain. Lua controls, other waveforms, filters, mixing, and full-range synthesis
remain later checkpoints.

The earlier pre-ADSR triangle at `1664043` passed desktop/Pi listening, gameplay,
and all 44 tests. Those results do not validate this new ADSR sequence.

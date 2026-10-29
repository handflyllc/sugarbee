# Standalone ADSR envelope

```sh
cmake --build build
ctest --test-dir build -R '^audio_envelope$' --output-on-failure
```

This checkpoint is pure C: no SDL, Lua, audio device, or triangle integration.
The library is built even with `SUGARBEE_BUILD_HOST=OFF`. There is nothing new
to listen to; both existing tone tools keep their previous behavior.

## Contract

- A zero-initialized envelope is idle at level zero.
- Start validates the entire settings request before changing state. Attack,
  decay, and release accept finite 0–10 seconds; sustain accepts finite 0–1.
- Settings are copied per note. Editing the caller's settings changes neither
  the current stages nor the later release. New settings apply on the next start.
- Start requires idle state. Active starts return false unchanged; retrigger
  semantics remain deferred rather than becoming part of this prototype.
- Timing is fixed to the engine's 48,000 frames/second. Positive durations
  round to the nearest frame, half upward, with a minimum of one frame.
- Attack rises linearly from zero to one; decay falls linearly to sustain.
  Sustain holds indefinitely, including when its level is zero.
- Note-off begins release from the current level using the captured duration.
  Repeated note-off during release and note-off while idle do nothing.
- Release reaches exactly zero, then becomes idle. Reset immediately clears
  state for initialization/cleanup; it is not an audible stop-fade operation.

`sb_envelope_advance()` advances one frame interval and returns its ending level.
The starting level is available in the state before advancing. A nonzero attack
of four frames therefore starts at zero and yields 0.25, 0.5, 0.75, and 1.0,
provided no immediate following stage changes that boundary level.

Zero-duration stages settle immediately: zero attack starts at one; zero decay
then moves directly to sustain. If both are zero, start immediately reaches
sustain. At the end of a positive attack followed by zero decay, that boundary's
returned value is already sustain. Zero release reaches idle immediately at
note-off. These are mathematical transitions; click reduction and how levels
are applied to oscillator samples belong to the next integration checkpoint.

The component performs no allocation, I/O, or locking. A future caller must
provide synchronization; no audio-thread ownership changes are made here.

## Coverage

`audio_envelope` has a five-second external timeout. It checks known linear
values and exact stage boundaries, long sustain, settings capture and next-note
edits, note-off at attack start/midpoint/peak and during decay, repeated note-off,
all combinations of zero durations with sustain zero/one, explicit immediate
transitions, frame rounding, invalid/nonfinite input without partial mutation,
NULL arguments, and finite bounded monotonic output at ten-second limits.

Desktop automated checks pass. Pi validation, triangle integration, audible
ADSR checks, active-note retrigger behavior, and an audible stop control remain
future work. No Lua API or configurable synthesizer is exposed.

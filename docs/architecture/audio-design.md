# SugarBee audio — working design

Status: design agreement only. Audio and the illustrated API are not implemented.
This document records discussion decisions; it is not an available API reference.
Names and signatures below are illustrative until implementation is reviewed.

## Purpose and scope

Provide expressive SID-inspired synthesis and bounded sampled-sound playback.
The SID is an inspiration, not an emulation or file-compatibility target.
SugarBee is not intended to become a DAW or a music composition editor.
External tools may eventually author compositions targeting SugarBee.

## Agreed synthesis model

- Initially provide exactly nine explicitly addressed synth voices.
- Each voice has independent waveform, envelope, filter, volume, and stereo pan.
- Left, center, and right are pan positions within two-channel stereo output,
  not permanently assigned groups of voices.
- Native engine code generates audio and advances envelopes on the audio timeline.
  Lua issues controls; game updates do not generate individual audio samples.
- Evaluate twelve or more voices later through performance and listening tests,
  including Raspberry Pi 4 testing. No expanded capacity is promised.

## Working Lua API shape

Use a dedicated synthesis namespace, numbered voices 1 through 9, and table-based
configuration. Voice numbers are independent of input-player slots and sample
playback voices.

```lua
sugarbee.synth.configure(1, {
    waveform = "pulse",
    pulse_width = 0.5,
    attack = 0.01,
    decay = 0.12,
    sustain = 0.6,
    release = 0.25,
    volume = 0.7,
    pan = -0.5,
    filter = {
        mode = "lowpass",
        cutoff = 1800,
        resonance = 0.3
    }
})

sugarbee.synth.note_on(1, 440)
sugarbee.synth.set_pitch(1, 660)
sugarbee.synth.set_pan(1, 0.5)
sugarbee.synth.set_volume(1, 0.4)
sugarbee.synth.set_filter(1, { cutoff = 700 })
sugarbee.synth.note_off(1)
sugarbee.synth.stop(1)
```

Configuration updates only supplied fields, including fields in nested tables.
Validate the entire request before changing anything. Invalid values and unknown
fields reject the entire request, with a diagnostic identifying the voice, field,
and problem. Configuration changes do not implicitly start or retrigger a note.
Exact error-delivery semantics remain to be finalized.

## Voice lifecycle

- Every voice starts silent with predictable settings.
- `note_on` starts the requested frequency in hertz. On an already sounding voice,
  it replaces the pitch and retriggers the envelope. The precise retrigger
  transition and smoothing remain implementation decisions.
- `set_pitch` changes frequency without retriggering the envelope.
- Overlapping synthesized notes require separate explicit voices.
- `note_off` starts release from the current envelope level. Repeating it during
  release does not restart the fade.
- `stop` bypasses the configured release duration, using a brief smoothing ramp
  to avoid an abrupt discontinuity.
- Releasing or stopping an already silent voice is harmless. Neither operation
  erases its configuration.

## Agreed starting settings

| Setting | Default |
| --- | --- |
| Waveform | Triangle |
| Pulse width | 0.5 |
| Attack | 0.01 seconds |
| Decay | 0.10 seconds |
| Sustain | 0.7 |
| Release | 0.15 seconds |
| Voice volume | 0.5 |
| Pan | 0.0, centered |
| Filter mode | Bypass |
| Filter cutoff | 2000 Hz |
| Filter resonance | 0.0 |

Retain pulse width while other waveforms are selected and retain filter settings
while bypassed. Sustain and voice volume use 0–1 values; pan uses -1 (left)
through 0 (center) to 1 (right). Envelope times are in seconds.

Triangle, saw, pulse, and noise are the initial waveform candidates. Noise
behavior, frequency and timing bounds, pulse-width limits, filter cutoff bounds,
and the normalized resonance mapping still require definition.

## Filtering

Provide an independent filter per voice, initially low-pass and bypass.
The conceptual signal path is oscillator → filter → envelope and volume → pan
→ mix. Cutoff is expressed in hertz. Smooth live parameter changes where needed.
Filter implementation and exact smoothing behavior remain open.
High-pass, band-pass, automatic filter envelopes, and modulation routing are
future discussion topics rather than initial requirements.

## Sample playback

WAV sound effects remain the tentative starting format. Define supported WAV
encodings, loading limits, asset lifetime, and playback controls before coding.

Repeated triggers may overlap using separate playback voices sharing loaded
sound data. Use a bounded sample-voice pool, separate from the nine synth voices.
When full, decline the new request and return a result the game can inspect.
Existing playback continues. Exhaustion is an expected condition, not a Guru
Meditation error or a reason to flood the console. The pool size and result
representation remain open.

## Agreed mixing policy

- Mix using floating-point accumulation.
- Provide per-voice volume, synth and sample group volumes, and master volume.
- Group and master controls use 0–1 values with smoothly applied changes.
- Master volume zero mutes output without stopping playback.
- Reserve fixed internal headroom based on supported voice budgets, not the
  instantaneous number of active voices. Starting a voice must not automatically
  turn down existing voices.
- Select actual attenuation through measurements and listening tests, including
  filter resonance and simultaneous sample playback.
- Clamp remaining out-of-range peaks at the final output boundary. This is
  protection and can distort an overloaded mix; it is not a desired effect.
- Make clipping detectable through a diagnostic counter rather than per-sample
  console messages. Counter access and reset semantics remain open.

Illustrative group controls:

```lua
sugarbee.audio.set_synth_volume(0.8)
sugarbee.audio.set_sample_volume(0.6)
sugarbee.audio.set_master_volume(0.7)
```

Fixed headroom does not guarantee that arbitrary resonant signals cannot exceed
the output range. Group/master defaults, panning law, filter gain handling, and
final attenuation remain to be specified. A more sophisticated limiter is deferred.

## Remaining implementation questions

- Audio backend, dependencies, device format, buffering, and latency.
- Thread-safe communication between Lua/game updates and audio processing.
- Behavior when no audio device is available or an active device fails.
- Resource ownership, cartridge teardown, and behavior during Guru Meditation.
- Parameter validation limits, smoothing, defaults not specified above, and
  exact error/result contracts.
- Tests for synthesis, bounds, mixing, lifecycle, and real hardware output.

## Deferred topics

- Externally authored SugarBee compositions containing sound definitions and
  timed events; format, scheduling, tooling, and voice ownership remain open.
- Actual C64 SID-file importing and chip emulation are outside the intended scope.
- MIDI and possible instrument-provider plugins are distant exploratory topics.
  A built-in general-purpose instrument bank is not a requirement.
- Higher synth voice counts and more advanced synthesis features.

# Synth audio (experimental v0.1)

Cartridges use `sugarbee.synth` and explicit voices **1–9**. Voices are independent
of controller players. The host generates samples; Lua only issues commands.
Calls are available at top level, in `init`, gameplay callbacks, and eligible
`shutdown`. New cartridges always start with silent, default voices.

```lua
sugarbee.synth.configure(1, {
    waveform = "pulse",
    attack = 0.01, decay = 0.1, sustain = 0.7, release = 0.15,
    volume = 0.5, pan = 0, pulse_width = 0.3,
    filter = {cutoff = 700, bypass = false}
})
local ok, reason = sugarbee.synth.note_on(1, 440)
-- Later, in an update:
sugarbee.synth.note_off(1)
```

## Calls and results

| Call | Effect |
| --- | --- |
| `configure(voice, settings)` | Update supplied fields without starting a note. |
| `note_on(voice, hz)` | Start/retrigger the selected triangle, saw, or pulse. |
| `note_on(voice)` | Start/retrigger selected noise; a frequency is an error. |
| `note_off(voice)` | Release using the note's captured release duration. |
| `stop(voice)` | Stop with a 10 ms fade, overriding the configured release. |

Successful calls return one value, `true`. Native state refusal returns
`false, "audio_rejected"`: for example, requesting a different waveform while a
waveform transition is still running, or issuing controls after session failure.
The rejected request changes nothing. A repeated request for the current
transition destination succeeds without extending the transition.

When no audio device opened, configuration still validates and retains settings,
returning `true`. Playback/release/stop return `false, "audio_unavailable"`.
Cartridges should continue gameplay. There is no reconnection or separate
availability query in this checkpoint.

Invalid arguments raise Lua errors. Numbers must be actual finite numbers,
not numeric strings. Voice indices must be integral and within 1–9. Extra or
missing arguments, unknown fields, wrong types, and out-of-range values are
errors even when audio is unavailable. An uncaught error follows normal Guru
handling; `pcall` can catch ordinary argument errors.

## Configuration

Only supplied fields change, including nested filter fields. An empty table is
valid. The complete request is validated before any configuration is published;
a bad field or native refusal leaves both retained settings and audio unchanged.
Only raw table entries are read; inherited fields and table metamethods are not
used. Settings are copied, so editing the table later has no effect.

| Field | Values | Default |
| --- | --- | --- |
| `waveform` | `"triangle"`, `"saw"`, `"pulse"`, `"noise"` | `"triangle"` |
| `attack`, `decay`, `release` | 0–10 seconds each | 0.01, 0.1, 0.15 |
| `sustain` | 0–1 | 0.7 |
| `volume` | 0–1 | 0.5 |
| `pan` | −1 (left) to 1 (right) | 0 (center) |
| `pulse_width` | 0.05–0.95 | 0.5 |
| `filter.cutoff` | 20–20,000 Hz | 2000 |
| `filter.bypass` | Boolean | `true` |

The filter is a simple low-pass. Setting its cutoff does not enable it; set
`bypass=false`. There is no `mode`, resonance, seed, or pitch configuration field.
Pitched note frequencies are **110–880 Hz**, including fractional frequencies.
Noise takes no frequency. Each voice uses its existing deterministic noise seed.

Envelope edits apply to the next note. A retrigger attacks from the current
level, preserves active oscillator/noise state, and captures the new envelope.
Release and stop do not restart when repeated. Idle release/stop succeed.
Volume, pan, pulse width, filter, and waveform changes can affect an active note
with the existing native smoothing. Zero envelope times can produce sharp edges.
To overlap notes, use separate voices. Nine-voice headroom and output gain are
unchanged; adding a voice does not reduce the others' volume.

## Host lifecycle

Pause fades to silence over 10 ms, freezes the complete bank, and resume fades
back in. Failure rejects controls and fades out. Eligible shutdown retains audio
access; paused shutdown stays silent. Teardown clears voices immediately without
waiting for notes to finish. See [lifecycle checks](../../tests/audio-session.md).

Separate pitch-only controls, WAV playback, music-file formats, and sequencing
helpers are not exposed here. The basic [Synth Lab](../../examples/synth-lab/README.md)
demonstrates the current API. A short public-domain musical passage is accepted
follow-up work using a Lua note/duration table, after this checkpoint is validated.

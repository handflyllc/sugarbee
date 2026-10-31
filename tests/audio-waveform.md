# Native waveform switching

This checkpoint adds explicit triangle/saw/pulse selection to the nine-voice
bank and locked SDL controls. Pitch remains 220–660 Hz. Noise transitions, broader
pitch, Lua controls, and cartridge-owned audio lifecycle are deferred.

## Selection and note-on

- `sb_audio_bank_waveform` / `sb_audio_voice_waveform` select a waveform without
  starting or retriggering an envelope. The shared enum is in
  `include/sb_audio_waveform.h`.
- Idle selection is immediate and silent. `sb_audio_bank_start_selected` /
  `sb_audio_voice_start_selected` start the selected pitched waveform: triangle
  phase .25, saw phase .5, pulse phase 0 with stored width (default .5).
- Existing type-specific note-ons retain their behavior and reject active
  cross-type starts. During any fade they reject all starts. Use start-selected
  to retrigger the selected destination during a fade; it preserves both phases
  and the deadline, captures the new ADSR, and updates both oscillators' pitch.
- Selecting noise is unsupported. Switching away from an active noise voice is
  also rejected. An idle former noise slot can select a pitched waveform.
- Invalid indices, enum values, and unsupported/busy requests return false
  without changing state. SDL wrappers also return false when audio is unavailable.

## Phase and fade contract

The incoming oscillator begins at the outgoing oscillator's normalized cycle
phase and current pitch. Both advance at the same pitch throughout the fade.
Phase preservation means cycle position, not equal waveform values. Entering
pulse applies its stored width immediately and initializes its edge history for
that pitch; subsequent width changes use the existing 10 ms ramp. Outgoing pulse
width edits also ramp while that source is still audible.

The 480-frame linear fade uses destination weights 0/480 through 479/480;
the next frame is destination-only. Source weights sum to one. The envelope,
volume, pan, and stop advance exactly once per frame after mono blending. There
is no extra voice allocation or envelope restart. Fixed 1/9 mixer headroom and
separate SDL diagnostic attenuation are unchanged. Linear blending preserves
bounds, not perceived loudness; phase cancellation can produce a temporary dip.
This is not a claim of click-free output under every combination of controls.

| Request | Outcome |
| --- | --- |
| Current waveform, no fade | Success, no change |
| Same destination during fade | Success, original deadline retained |
| Other destination during fade, including outgoing waveform | Failure, no change or queue |
| New destination after completion | New 480-frame fade |

Pitch edits affect both sources without resetting phase. Release and stop remain
available; if the envelope ends before the fade, the fade is discarded and its
destination retained for the next note. Zero-length release does this immediately.
Muted voices still advance. Bank reset cancels everything. Rendering zero frames
does not advance a fade. Buffer partitioning does not change output.

## Automated and manual checks

```sh
ctest --test-dir build -R 'audio_waveform|audio_bank_sdl' --output-on-failure
./build/sbee-waveform-test
```

The pure test covers all six switch directions at pitch extremes, exact blend
samples and deadlines, phase continuity, no-op/rejection atomicity, shared ADSR
and controls, pitch edits, selected retrigger, width edits on either side,
release/stop, mute, nine-voice bounds, chunk independence, and reset. Existing
triangle/noise/saw/pulse tests protect non-switching behavior. SDL regression
checks compare the actual callback against the bank, verify control routing and
legacy diagnostic exclusion, and destroy the device during a transition.
The timed dummy-device listening executable verifies completion, not audible quality.

The listening sequence holds a centered 330 Hz note while visiting all six
transition directions, then changes nine stereo-spread voices from triangle to
saw to pulse and releases them. Listen for sustained pitch, expected timbre and
loudness changes, no unexpected harsh clicks or distortion, and normal exit.
All 63 automated tests pass. The user confirmed clean desktop waveform
transitions, steady pitch, and normal exit. Pi acceptance remains pending.

A pure build requires neither SDL nor Lua:

```sh
cmake -S . -B /tmp/sugarbee-waveform-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-waveform-standalone --target test_audio_waveform
ctest --test-dir /tmp/sugarbee-waveform-standalone -R '^audio_waveform$' --output-on-failure
```

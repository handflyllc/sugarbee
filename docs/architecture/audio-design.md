# SugarBee audio — settled initial design

Status reconciled with the implementation on 2026-10-31. The initial design was
settled on 2026-10-18; implementation decisions below refine that design.
This document separates the current native implementation from the planned Lua
API and remaining synthesis/WAV work. Illustrative Lua calls are not available
APIs. Historical checkpoint notes at the end describe their state at the time,
not current limitations.

## Current implementation

| Area | Implemented now | Still pending |
| --- | --- | --- |
| Output | Host-owned SDL2 device, 48 kHz float stereo, silent fallback, clean close | Live failure detection/reconnection, measured end-to-end latency |
| Voice bank | Nine explicit native slots, triangle, white noise, saw, or pulse, independent state; 10 ms triangle/saw/pulse transitions | Noise transitions |
| Triangle | Direct waveform, 220–660 Hz, phase-preserving pitch/retrigger | Aliasing control and broader supported pitch range |
| Saw | PolyBLEP, 220–660 Hz, native bank/SDL integration and automated checks | Broader pitch |
| Pulse | Bank/SDL PolyBLEP, 220–660 Hz, width 0.05–0.95, default 0.5, 10 ms ramps, explicit DC conditioning | Broader pitch |
| Noise | Deterministic per-sample white noise, distinct default seeds, repeatable idle starts | Rate-controlled/tonal noise remains deferred; no current noise frequency control |
| Envelope | Linear ADSR, captured per note, current-level active retrigger | No additional envelope model planned for this checkpoint |
| Controls | Volume 0–1, equal-power stereo pan -1–1, 10 ms smoothing, 10 ms short stop | Filters, group/master controls |
| Mixing | Fixed 1/9 synth headroom with double-precision accumulation | WAV mixing, resonance gain policy, final clamp/clip diagnostics |
| Integration | Locked native controls and separate listening executables | Lua audio API, cartridge-owned audio cleanup and pause/error policy |
| WAV | Design only | Decoder, bounded asset storage, playback voices, Lua controls |

Current bank diagnostic attenuation is separately 0.790569415, chosen for
listening audibility. Legacy single-triangle and sine diagnostics retain their
0.05 level. These are test levels, not a finalized master-volume API. Current
triangle/noise mixing stays within ±1 under supported controls; there is no
final clamp or clipping counter yet. Filters and WAV mixing need new bounds.

The user confirmed successful Pi validation at `f1982a9`: all 56 automated
tests, clean native saw and earlier audio sequences, gameplay, and Exit to system
status 0. Desktop validation also passed. This does not establish compatibility
with every audio device or live output switching. See
[saw validation](../../tests/audio-saw.md#pi-validation) and
[Pi output observations](../../tests/audio-bank.md#pi-validation-and-output-observations).

## Purpose and scope

Provide expressive SID-inspired synthesis and bounded sampled-sound playback.
The SID is an inspiration, not an emulation or file-compatibility target.
SugarBee is not intended to become a DAW or a music composition editor.
External tools may eventually author compositions targeting SugarBee.

## Target synthesis model

- Initially provide exactly nine explicitly addressed synth voices.
- Each voice has independent waveform, envelope, filter, volume, and stereo pan.
- Left, center, and right are pan positions within two-channel stereo output,
  not permanently assigned groups of voices.
- Native engine code generates audio and advances envelopes on the audio timeline.
  Lua issues controls; game updates do not generate individual audio samples.
- Evaluate twelve or more voices later through performance and listening tests,
  including Raspberry Pi 4 testing. No expanded capacity is promised.

## Planned Lua API shape — not implemented

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
Out-of-range values are rejected rather than silently clamped. Exact synth
error-delivery and success-return semantics remain to be finalized.

## Current native voice lifecycle

- Triangle, noise, and saw share linear ADSR, volume, pan, and short-stop behavior.
- Idle saw start begins at phase 0.5. Active saw retrigger and pitch-only edits
  preserve phase, with the same envelope/retrigger rules as triangle.
- Idle triangle start begins at phase 0.25. Active triangle note-on preserves
  phase and changes pitch. Pitch-only changes do not retrigger the envelope.
- Active note-on attacks from the current envelope level to one over the full
  newly captured attack duration, including during release or short stop. It
  cancels a pending stop; an attack from level one holds for that duration.
- Each note captures its ADSR settings. Configuration edits alone do not alter
  the current note's envelope. Zero-duration stages are immediate, so arbitrary
  envelope settings are not guaranteed click-free.
- Idle noise start resets to its configured seed. Active noise retrigger keeps
  its sequence; seed edits apply only on the next idle noise start. Seed zero
  selects the slot's distinct fixed default. Muting, release, and short stop
  keep advancing the sequence until idle. See [seed rules](../../tests/audio-noise.md#native-bank-integration).
- Volume and pan targets smooth over 480 frames (10 ms); idle edits apply
  immediately. Retrigger retains active control ramps; mute does not stop time.
- Note-off releases from the current level using captured release settings.
  Repeated note-off does not restart release. Stop overrides release with a
  480-frame fade; repeated stop/note-off cannot prolong that stop.
- Idle slots can start any implemented type. Active cross-type starts are
  rejected. Noise has a separate frequency-free note-on and rejects pitch edits.
- Bank reset cancels notes and restores defaults; it is cleanup, not a fade.
  Normal host startup remains silent; cartridges cannot request audio yet.

## Target voice lifecycle — remaining API work

These Lua-facing behaviors include already implemented native foundations and
features still to be added. Current native restrictions above remain in force.


- Every voice starts silent with predictable settings.
- `note_on` starts the requested frequency in hertz. On an already sounding voice,
  it replaces the pitch and retriggers the envelope using the current-level,
  full-attack-duration rule implemented natively. The Lua noise call shape
  must be reconciled with the current frequency-free white-noise operation.
- `set_pitch` changes frequency without retriggering the envelope.
- Each `note_on` captures attack, decay, sustain, and release settings for that
  note, including its later release. Envelope configuration edits apply to the
  next note; a sustained note may continue indefinitely until released or stopped.
- Pitch, volume, pan, and filter changes affect the current sound.
- Waveform and pulse-width changes also affect the current sound without
  retriggering its envelope. Smooth pulse-width changes and use a brief waveform
  transition to reduce clicks. Native triangle/saw/pulse selection now uses the
  10 ms phase-preserving contract documented below; noise transitions are deferred.
- Overlapping synthesized notes require separate explicit voices.
- `note_off` starts release from the current envelope level. Repeating it during
  release does not restart the fade.
- `stop` bypasses the configured release duration, using a brief smoothing ramp
  to avoid an abrupt discontinuity.
- Releasing or stopping an already silent voice is harmless. Neither operation
  erases its configuration.

## Target starting settings

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

Pulse width and 10 ms live ramps are implemented in the native bank/SDL controls.
Width is retained while other waveforms are selected; idle pulse starts apply the
stored target immediately. Filter fields remain unimplemented; retain filter settings
while bypassed. Sustain and voice volume use 0–1 values; pan uses -1 (left)
through 0 (center) to 1 (right). Envelope times are in seconds.

The target waveform set is triangle, saw, pulse, and noise. The original design
assigned noise frequency to the rate of random-value changes. The accepted first
noise implementation instead generates a new white-noise value every audio
sample and has no frequency parameter. Rate-controlled or tonal noise is a
separate deferred discussion, not a missing control to expose automatically.
Envelope, volume, and pan already apply to noise; filtering is still future work.

## Current and target parameter limits

| Parameter | Current native implementation | Target / deferred design |
| --- | --- | --- |
| Synth voice number | Integers 1–9 | Same |
| Triangle pitch | 220–660 Hz | 20–20,000 Hz, strictly below half the sample rate; requires aliasing work |
| White noise | New value every sample at 48 kHz; no pitch argument | Rate-controlled noise deferred; original 20–20,000 Hz change-rate proposal is not implemented |
| Noise seed | uint32; zero selects a distinct fixed default per bank slot | Lua seed exposure undecided |
| Pulse width | Native bank/SDL: 0.05–0.95, 10 ms live ramps | Lua controls pending |
| Attack, decay, release | 0–10 seconds each | Same |
| Sustain and voice volume | 0–1 | Same |
| Pan | -1–1, equal-power stereo | Same |
| Filter cutoff | Not implemented | 20–20,000 Hz, strictly below half the sample rate |
| Resonance | Not implemented | 0–1; mapping and stability limits unresolved |

Reject non-finite values (NaN and infinity), invalid voice indices, and values
outside these ranges. Controls accept fractional values, not only tenths;
precision follows the implementation's floating-point representation.
Zero envelope duration is implemented as an immediate stage transition; this
deliberately does not promise click-free arbitrary settings. The
10-second limit applies to transitions, not total note duration.

The planned WAV path will support longer ambience through whole-sound loops; externally authored scores remain deferred.
The foundation uses a 48,000 Hz internal sample rate. Any additional numerical
stability margin for the filter must be specified and tested before publishing
a concrete synthesis API contract.

## Filtering — not implemented

Provide an independent filter per voice, initially low-pass and bypass.
The conceptual signal path is oscillator → filter → envelope and volume → pan
→ mix. Cutoff is expressed in hertz. Smooth live parameter changes where needed.
Filter implementation and exact smoothing behavior remain open.
High-pass, band-pass, automatic filter envelopes, and modulation routing are
future discussion topics rather than initial requirements.

## Sample playback — not implemented

Use eight WAV playback voices, separate from the nine synth voices. These are
initial budgets to validate on hardware, not measured Pi performance claims.
A loaded asset holds shared decoded data; each successful playback receives a
separate opaque handle with its own position, volume, and pan.

Illustrative API:

```lua
local sound = sugarbee.audio.load("assets/laser.wav")
local playback, reason = sugarbee.audio.play(sound, {
    loop = false,
    volume = 0.5,
    pan = 0.0
})
sugarbee.audio.is_playing(playback)
sugarbee.audio.set_pan(playback, 0.5)
sugarbee.audio.set_volume(playback, 0.4)
sugarbee.audio.stop(playback)
local released, reason = sugarbee.audio.unload(sound)
```

The illustrated playback options are the defaults. Repeated triggers may overlap.
When all eight slots are occupied, `play` returns `nil, "voices_full"`; existing
sounds continue. This is an expected result, not a Guru error or console flood.
Invalid asset handles, malformed arguments, and out-of-range controls raise
descriptive Lua errors. Ended playback handles are treated differently:

- `is_playing` returns false after playback finishes.
- Volume and pan setters return false for ended playback, true when applied.
- An old playback handle never controls a new sound that reuses its slot.
- Invalid arguments remain errors even when a playback has ended.

### Playback and stopping states

| State | `is_playing` | `stop` |
| --- | --- | --- |
| Playing | true | Begins a brief smoothing fade; returns true |
| Already stopping | true | Returns false without restarting the fade |
| Finished | false | Returns false |

A stopping playback retains its slot and keeps its asset in use until the fade
finishes. Exact fade duration and control changes during stopping remain open.

One-shot playback frees its slot at the end. Whole-sound loops retain one slot
until stopped or the session ends. Loop boundaries are not automatically
crossfaded; authors should prepare seamless loops. Seeking, pause/resume,
playback-speed changes, and custom loop regions are deferred.

`unload` returns `false, "asset_in_use"` while any playback uses the asset;
otherwise it releases the asset and returns true. Cartridge teardown stops its
playbacks and releases its assets.

### Initial WAV support

- Uncompressed signed 16-bit PCM WAV, mono or stereo.
- Sample rates of 22,050, 44,100, or 48,000 Hz.
- Convert supported assets to a common internal format at load time.
- Load the complete decoded sound into memory; streaming is deferred.
- Mono pan distributes sound between stereo outputs. Stereo pan acts as balance:
  center preserves both channels, left attenuates right, and right attenuates left.
  The exact pan/balance gain curves remain to be specified.

Missing, malformed, or unsupported files raise descriptive Lua errors.

### Initial memory budgets

| Resource | Testing limit |
| --- | --- |
| One decoded WAV asset | 4 MiB |
| All loaded WAV assets | 32 MiB |
| Loaded WAV asset count | 64 |
| Simultaneous WAV playbacks | 8 |

Size limits apply after conversion to the internal format. Overlapping playbacks
share the asset data. Exceeding the total memory or asset-count budget returns
`nil` and a checkable reason (reason strings remain to be chosen). Exceeding the
per-asset limit raises an error identifying that limit.

These memory budgets are for initial testing, not permanent platform limits.
Incoming file-size limits, temporary conversion-memory bounds, and compatibility
with the existing cartridge asset-reader limits must be checked before coding.
Longer WAV background music is a future use case: evaluate increased limits
versus streaming rather than assuming every track fits the initial budget.

## Audio availability and cartridge lifecycle

Implemented now: startup failure permits silent gameplay with one diagnostic;
the host retains the device across cartridge sessions and closes it after
eligible Lua shutdown on host exit. Native control calls return false when
audio is unavailable. Device access is serialized with SDL locking. There is
no Lua audio query, WAV loading, or cartridge-generated audio yet.

The remainder of this section is the target contract. Cartridge-owned voice
cleanup and audio behavior on pause/resume/Guru must be implemented and tested
before Lua audio is exposed.


Unavailable audio allows silent gameplay and produces one explanatory diagnostic.
Expose availability to Lua; the query name remains undecided. Asset loading still
validates and prepares sounds, and synth configuration remains usable.
WAV playback returns `nil, "audio_unavailable"`; synth note-on returns
`false, "audio_unavailable"`. Automatic device reconnection is deferred;
restoring audio initially requires restarting SugarBee.

When Guru Meditation activates, stop WAV and synth voices with a brief fade and
reject new playback while the cartridge is failed. Exact refusal results remain
open. This prevents sounds from continuing indefinitely after game logic fails.

On normal application exit, keep audio available during Lua's `shutdown()` callback.
Then stop voices and release assets in native teardown; do not wait for sounds
started by that callback to finish. Fade completion and device teardown ordering
must be implemented without an unbounded wait.

## Target mixing policy

Implemented subset: floating-point mixing, per-voice volume and equal-power pan,
fixed 1/9 synth headroom, and separate diagnostic gain. Group/master controls,
WAV mixing, final clamp, and clipping diagnostics below remain unimplemented.


- Mix using floating-point accumulation.
- Provide per-voice volume, synth and sample group volumes, and master volume.
- Synth group, WAV group, and master volume each default to 1.0.
- Group and master controls use 0–1 values with smoothly applied changes.
  A value of 1.0 means no additional attenuation at that control, not maximum
  hardware loudness. Per-voice default volume remains 0.5, and fixed internal
  headroom attenuation applies independently.
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
the output range. Synth equal-power panning is implemented; WAV pan/balance,
filter gain handling, and combined-output attenuation remain to be specified. A more sophisticated limiter is deferred.

## Remaining implementation questions

- Native pulse bank/SDL integration with explicit DC conditioning is validated
  on desktop/Pi. Measure quality before expanding pitch ranges.
- Noise waveform transitions; native triangle/saw/pulse switching is implemented.
  White noise versus any future rate-controlled noise API.
- Filter mathematics, resonance mapping, stability, smoothing, and gain bounds.
- Lua result/error contracts, configuration atomicity, and noise call/seed shape.
  Native controls already synchronize with the callback through SDL device locks.
- Cartridge voice ownership, pause/resume, Guru recovery, and bounded teardown.
- Group/master controls, WAV balance, combined headroom, final clamp, and clipping
  diagnostic access/reset rules.
- WAV decoding, asset/playback handles, validation/refusal precedence, stopping
  controls, and bounded source/temporary memory compatible with asset limits.
- Live audio-device failure handling; reconnection remains deferred. Measure
  latency and Pi performance for each added slice rather than assuming them.

Implemented decisions are no longer open: 48 kHz internal output, linear ADSR,
per-note capture, current-level retrigger, 10 ms stop and volume/pan ramps,
equal-power synth pan, fixed 1/9 current synth headroom, and noise seed rules.

## Agreed next sequence — checkpoint by checkpoint

1. PolyBLEP saw and bank/SDL integration are implemented at 220–660 Hz.
   Desktop and Pi validation passed; broader range requires a separate checkpoint.
2. Pulse bank/SDL and width controls are validated on desktop/Pi. Native
   triangle/saw/pulse transitions are implemented and desktop listening passed;
   Pi acceptance is pending. Noise transitions and broader pitch need separately reviewed checkpoints.
3. Per-voice low-pass/bypass, with resonance and gain bounds settled before use.
4. Cartridge audio lifecycle and minimal Lua controls for validated features.
5. A small Synth Lab, then a narrow Pong Lab audio update: one short wall-hit
   blip and a distinguishable paddle-reflection blip. Keep gameplay unchanged;
   this is an API-use/acceptance exercise, not a soundtrack or game redesign.
6. Bounded WAV playback as a separate milestone.

Each stage needs its own scope/acceptance review; this sequence is not permission
to implement all stages at once. Saw integration retains the bounded pitch range;
PolyBLEP is alias-suppressed, not an alias-free promise.

## Deferred topics

- Externally authored SugarBee compositions containing sound definitions and
  timed events; format, scheduling, tooling, and voice ownership remain open.
- Actual C64 SID-file importing and chip emulation are outside the intended scope.
- MIDI and possible instrument-provider plugins are distant exploratory topics.
  A built-in general-purpose instrument bank is not a requirement.
- Higher synth voice counts and more advanced synthesis features.

## Historical checkpoint record

The following dated or named checkpoints preserve the sequence of decisions and
validation. Statements such as "not implemented" or "future work" describe
that checkpoint only. Use the current summary above for present capabilities.

## Hiatus checkpoint — 2026-10-18

The initial audio design discussion is settled. Resume by reviewing the engineering
questions above and choosing one small implementation slice; do not treat all
nine-voice synthesis and WAV playback as one coding iteration. No audio runtime
code has been added or tested as part of this design work.

Pong Lab remains paused pending two-controller access. Persistent logging and
Lua sandboxing/execution limits remain separate engine work deferred until Pong
is complete. Pi DEV/Console deployment and audio performance require hardware
access. This checkpoint does not schedule those items or revive deferred MIDI,
composition-format, or advanced synthesis work.

## First implementation checkpoint: silent host output

The host now owns an SDL2 audio backend independently of cartridge sessions.
After graphics and input initialize, it attempts to initialize audio and open
the default playback device. Initialization or device-open failure emits one
explanatory diagnostic and leaves audio unavailable without stopping gameplay.
No per-frame retries or automatic device reconnection are introduced.

The callback format is 48,000 Hz, native-endian float32, two interleaved stereo
channels. The requested buffer is 1,024 frames (about 21.3 ms of audio, not a
measured end-to-end latency). Only callback buffer-size changes are allowed;
SDL handles conversion when the hardware format differs. The callback fills
its entire buffer with zero-valued silence. It allocates nothing, logs nothing,
and accesses neither Lua nor mutable game state.

Cartridge exit/relaunch preserves the device. On host exit, eligible Lua
shutdown runs first; native teardown then closes the device, releases its SDL
audio subsystem reference, and finally the host calls SDL_Quit. SDL close waits
for the callback to finish; there is no application drain loop. This is not a
hard wall-clock guarantee against a stalled OS/audio driver.

The [foundation checks](../../tests/audio-foundation.md) exercise silence,
unavailable initialization, injected open failure, session lifetime, and clean
teardown using SDL's dummy drivers. All 40 automated tests passed. The user
confirmed the interactive Pong/Save Lab checks and one-message unavailable-audio
fallback without gameplay disruption. Pi validation passed at `d6c0dd9`. Audible-output validation is recorded
in the diagnostic-tone checkpoint below.
No test tone, synthesizer, WAV loading, Lua audio API, live failure detection,
or cartridge fade/mixing behavior is implemented in this checkpoint.

SDL reference: [opening a device](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice)
and [closing a device](https://wiki.libsdl.org/SDL2/SDL_CloseAudioDevice).

## Native diagnostic-tone checkpoint

The separate `sbee-audio-test` executable uses the same backend to request one
440 Hz sine tone: one second at 48 kHz, amplitude 0.05, identical stereo channels,
and ten-ms linear fades included in the duration. A small fixed oscillator
advances by frame position independently of callback size. This is a diagnostic
signal, not the nine-voice synth or a new agreed synthesis waveform.

Native requests and queries synchronize with the callback using the SDL device
lock. Requests while active are declined without restarting; unavailable audio
returns false. No Lua interface or normal-host automatic tone is introduced.
See [test instructions and limits](../../tests/audio-tone.md) for sample checks,
listening instructions, and bounded tool wait behavior. All 42 automated tests
passed. The user confirmed clean desktop output on the intended device, no clicks
or distortion, and normal exit. On Pi commit `d6c0dd9`, the user confirmed all
42 tests, clean tone output, gameplay and unavailable-audio checks, and Exit to
system status 0. The Pi initially selected unconnected A/V Audio Out; selecting
HDMI resolved silence. See the [troubleshooting notes](../../tests/audio-tone.md#pi-troubleshooting-no-audible-tone).

## Single native triangle prototype

One native triangle voice now supports start, phase-preserving pitch change,
and stop, at fixed quiet volume with ten-ms linear fades. This deliberately
bounded prototype accepts 220–660 Hz only; it is not the full synthesis API or
an alias-free oscillator. The direct waveform's aliasing and the agreed full
frequency range remain implementation work. Active start is declined, rather
than defining the future envelope-retrigger behavior prematurely.

The separate `sbee-triangle-test` requests 220, 440, and 660 Hz, then stops.
Requests synchronize through the SDL device lock. The sine diagnostic and
triangle are mutually exclusive; no mixer or Lua controls are added. Native
sample and dummy-output checks pass as part of all 44 automated tests. The user
confirmed clean desktop triangle tones, no obvious clicks at pitch changes, and
normal exit. On Pi commit `1664043`, the user confirmed all 44 tests passed,
clean triangle and original sine tones, normal gameplay checks, and Exit to
system status 0. This validates the bounded native prototype on desktop and Pi,
not full-range or nine-voice synthesis. See
[prototype tests and listening instructions](../../tests/audio-triangle.md).

## Standalone ADSR checkpoint

A pure-C envelope component now captures validated per-note settings and advances
linear attack, decay, sustain, and release at 48 kHz. Durations accept 0–10 seconds
and sustain accepts 0–1; nonfinite values reject the complete request unchanged.
Positive durations round to the nearest frame (half upward, minimum one), while
zero stages transition immediately. Note-off releases from the current level;
repeated note-off does not restart release. Sustain has no duration limit.

The original start operation remains idle-only; a separate retrigger operation
now captures new settings and attacks from the current level. Triangle
integration is described below. Reset remains initialization/cleanup, not an
audible stop fade. See the [timing contract and edge-case tests](../../tests/audio-envelope.md)
for exact boundary semantics. All 45 desktop tests pass, and the user confirmed clean ADSR playback, smooth
transitions, and normal exit. Pi ADSR validation passed at `710b738` as recorded below.

## Native triangle ADSR integration

The triangle now captures the agreed ADSR defaults or a validated complete native
configuration at each note-on. Active note-on preserves oscillator phase, changes
pitch, and attacks from the current level to one over the full new attack time.
At level one, that is a hold for the attack duration. The rule applies during
attack, decay, sustain, release, or short stop; retrigger cancels a pending stop.
Settings edits alone leave the current note and its captured release unchanged.

Note-off uses configured release from the current level; stop instead uses a
separate ten-ms fade. Repeated stop/note-off cannot extend an active stop. Zero
ADSR durations remain truly immediate, so arbitrary settings are not promised
click-free. The renderer samples the current level before advancing one frame.

The updated [native listening sequence](../../tests/audio-triangle.md) and tests
cover ADSR, pitch-only changes, retrigger during release, and short stop overriding
a long release. All 45 automated tests pass. The user confirmed clean desktop
ADSR playback, smooth transitions, and normal exit. On Pi commit `710b738`, the
user confirmed all 45 tests, clean ADSR/retrigger and sine output, gameplay
checks, and Exit to system status 0. This validates the bounded native ADSR
integration on desktop and Pi. Pitch remains limited to 220–660 Hz, volume/pan
fixed, and Lua deferred.

## Native volume and pan checkpoint

The single triangle now has native volume 0–1 (default 0.5) and equal-power pan
-1–1 (default zero). The fixed quiet testing attenuation of 0.05 remains separate.
Left/right gains are cosine/sine of `(pan+1)*pi/4`, with exact endpoint silence
and equal center gains. This replaces the prototype's full signal in both
channels; the standalone sine diagnostic is unchanged.

Active volume/pan targets ramp independently over 480 frames (10 ms) from their
current values. Pan gains are derived from the interpolated pan, retaining
constant squared gain sum through the transition. New targets restart from the
current value; repeated identical targets do not prolong ramps. Idle controls
apply immediately; idle restart retains targets. Active retrigger preserves the
ramps. Volume zero mutes without pausing waveform or ADSR progression.

Whole scalar requests reject invalid/nonfinite values unchanged. Native controls
use SDL device locking; no Lua API or additional voice is added. See
[automated checks and listening instructions](../../tests/audio-pan.md). Desktop
automated checks pass (47 tests). The user confirmed correct pan direction,
mostly smooth volume changes, silent mute, and normal exit on desktop. Pi
validation passed at `aa4f408`: the user confirmed all 47 tests, pan/volume,
earlier tones, gameplay, and Exit to system status 0. Perfectly smooth
transitions are not claimed. HDMI is preferred for future Pi tests; degraded
A/V output was observed with no established cause and is not an acceptance
requirement for this milestone. See the [output observation](../../tests/audio-pan.md#pi-output-observation). Full-range
aliasing control and final multi-voice headroom remain future work.

## Standalone nine-voice bank checkpoint

The pure C bank owns nine independent triangle voices, numbered 1–9. Each keeps
its own ADSR, phase, pitch, volume, pan, and smoothing state. It sums normalized
stereo voice output with double-precision accumulation and applies a fixed 1/9
synth gain. Starting or stopping another voice does not change an existing
voice's gain. Nine maximum-level coherent triangles remain bounded by ±1 per
output channel; there is no clamp or active-count normalization here. Additional
waveforms, filters, and WAV mixing will require reviewing that bound.

Quiet diagnostic attenuation (0.05) is applied separately by the existing
single-voice SDL diagnostic path. The bank itself has no diagnostic gain, SDL,
Lua, allocation, or device ownership. Existing diagnostic loudness is preserved.
The bank has not yet been connected to SDL or validated through listening/Pi
checks. See [the native contract and automated checks](../../tests/audio-bank.md).

## Native bank SDL checkpoint

The bank now renders through the existing SDL device and callback. Per-slot
native controls lock that device; the callback advances voices without
allocation, logging, or Lua. The bank's fixed 1/9 headroom is retained, followed
by separate approximately 0.791 bank diagnostic attenuation. Legacy diagnostics keep their existing
loudness. Bank playback and the legacy sine/single-triangle diagnostics refuse
conflicting note-on requests while the other path remains active.

The [bank listening sequence and regression tests](../../tests/audio-bank.md)
cover one, three, and nine voices, selective releases, short stops, and silence.
Dummy-output tests do not establish listening quality or Pi performance.
The user confirmed clean desktop listening at the final gain; all 50 automated
tests pass. On Pi commit `d71e42d`, all 50 tests, bank/earlier tones, gameplay,
and Exit to system status 0 passed. See the [output observations](../../tests/audio-bank.md#pi-validation-and-output-observations)
for preliminary Bluetooth success and HDMI port-change behavior. Normal host startup is silent;
Lua controls and cartridge-owned audio cleanup are not part of this checkpoint.

## Standalone white-noise checkpoint

A pure C mono noise source now advances a private xorshift32 state once per
sample. Seeded resets reproduce the exact sequence; zero seed/zero initialization
select default seed 1. Upper 24 bits map to symmetric values strictly inside
[-1, +1]. It has no diagnostic attenuation, envelope, bank/SDL integration, or
Lua API. Buffer partitioning does not change the output sequence.

See [the seed/reset contract and automated checks](../../tests/audio-noise.md).
Per-voice seed and retrigger policy, bank integration, listening, and Pi
validation remain future work. The statistical checks are regression checks,
not a proof of ideal whiteness.

## Noise bank integration checkpoint

Native bank slots now support triangle or white noise, sharing ADSR, volume,
pan, and stop behavior. Each slot has a distinct fixed default seed. Idle noise
starts replay the configured seed; active retriggers preserve the sequence and
attack from the current level over the full new attack duration. Seed edits
wait for the next idle start. Muting, release, and short stop continue sequence
advancement until idle. Bank reset restores defaults. Active cross-type changes
and noise pitch updates are rejected; noise has its own frequency-free note-on.

Fixed 1/9 headroom and separate bank diagnostic attenuation remain unchanged.
[Regression tests and the native listening sequence](../../tests/audio-noise.md)
cover seeds, retrigger, mixed playback, and cleanup. All 53 automated tests pass.
The user confirmed clean desktop noise playback, correct stereo placement, and
normal exit. The user reported that the Pi checklist for `557b6ba` passed
without issues: all 53 tests, noise/earlier audio, gameplay, and Exit to system
status 0. Lua controls and cartridge-owned audio remain deferred.

## Standalone saw checkpoint

A pure C PolyBLEP saw now supports 220–660 Hz at 48 kHz. Reset starts at the
rising zero crossing; pitch edits preserve phase. There is no envelope or gain
inside the oscillator. Sample/boundary tests and a coherent spectral comparison
against a raw ramp pass; the three measured frequencies show approximately
16 dB less unwanted power without simply attenuating the fundamental.
See [the exact tests and measurement limits](../../tests/audio-saw.md).
Bank integration, listening, Pi validation, and broader pitch remain deferred.

## Saw bank integration checkpoint

The native bank/SDL path now supports saw alongside triangle and noise. Idle
saw notes start at phase 0.5; active retrigger preserves phase and applies the
shared current-level/full-duration ADSR attack. Pitch edits preserve phase and
envelope. Type changes require an idle slot. Mute, release, and short stop keep
phase advancing until idle. Fixed 1/9 headroom and diagnostic gain are unchanged.

[Bank regression checks and the native listening sequence](../../tests/audio-saw.md#native-bank-and-sdl-integration)
cover lifecycle, headroom, mixed rendering, controls, and teardown. All 56
automated tests pass. The user confirmed clean desktop saw playback, correct
stereo placement, and normal exit. On Pi commit `f1982a9`, the user confirmed
all 56 tests, clean saw/earlier tones, successful gameplay, and Exit to system
status 0. Broader pitch and Lua controls remain deferred.

## Standalone pulse checkpoints

A pure C PolyBLEP pulse now runs at 48 kHz with pitch 220–660 Hz and width
0.05–0.95 (default 0.5). Reset begins at the corrected rising edge; pitch changes
preserve phase and width. Live width edits ramp over 480 samples (10 ms),
retargeting from the current width without resetting phase. Raw output stays within ±1
without clipping or normalization; its expected DC average is `2*width - 1`.
That offset remains available in raw output. A separate conditioned renderer
applies `0.5 * (raw - (2*current_smoothed_width - 1))`. This fixed gain
halves square amplitude and removes the settled expected mean, without promising
zero mean over short transition windows. Conditioning uses the current sample
width before ramp advancement, with output
bounded to ±0.95. Both render variants advance identical state; there is no
DC-blocking filter or live mode crossfade.

[Pulse tests and measured limits](../../tests/audio-pulse.md) cover reset,
bounds, raw and conditioned DC means, transition conditioning, static alias
suppression, exact width ramps, moving-edge correction, and comparison against an oversampled PWM reference. Bank
integration is now implemented and desktop/Pi validation passed.

## Native pulse integration checkpoint

The bank routes conditioned pulse output through shared ADSR, volume/pan, and
short stop, with fixed 1/9 headroom and separate SDL diagnostic gain. Idle pulse
starts reset phase to zero and apply stored width (default .5); active retrigger
preserves phase and width ramp. Width is retained across other waveform choices.
Active cross-type starts remain rejected. `sbee-pulse-test` exercises pitch,
width sweep, stereo placement, retrigger, and a nine-voice mix. See the
[pulse contract and checks](../../tests/audio-pulse.md#native-bank-and-sdl-integration).
All 61 automated tests pass. The user confirmed all desktop listening checks,
including stereo placement, pitch/width/retrigger playback, mixed voices, and
normal exit. On Pi commit `a397e26`, the user confirmed all 61 tests, clean
pulse/earlier tones, gameplay checks, and Exit to system status 0. Lua and
cartridge-owned audio lifecycle remain later work.

Raspberry Pi OS Lite 64-bit validation at `51cbc8b` passed all 61 tests, clean
sine/pulse audio with correct stereo placement and normal exits, responsive
Pong/Pac-Man, Save Lab persistence through relaunch/repacking, and Exit to system
status 0. Keyboard and controller had felt delayed in the desktop session but
responsive in the TTY; the delay was not reproduced on Lite. Its exact cause
remains undiagnosed. No latency measurement or causal link to audio is claimed.

## Native waveform-switch checkpoint

Triangle/saw/pulse selection is separate from note-on. Idle selection is silent;
active selection aligns normalized phase and pitch and blends for 480 frames
before shared ADSR/volume/pan. Repeating the destination does not restart a fade;
other requests during it fail without mutation or queuing. A selected-waveform
start supports retrigger during the fade, preserving phases and deadline.
Existing type-specific note-ons reject starts during fades. Pitch changes affect
both sources; envelope completion cancels the fade and retains its destination.
No noise transitions, extra headroom normalization, or Lua controls are added.
See [waveform tests and listening procedure](../../tests/audio-waveform.md).
All 63 automated tests pass. The user confirmed clean desktop transitions,
steady pitch, and normal exit. Pi validation remains pending.

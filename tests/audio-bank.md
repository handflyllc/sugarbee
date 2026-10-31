# Standalone nine-voice bank

The pure C bank is now connected to the SDL backend through native controls.
A separate listening executable exercises it; no Lua API is exposed.

## Contract

- Native voice numbers are 1–9. Each has independent triangle, ADSR, pitch,
  volume, pan, retrigger, and short-stop state.
- Existing prototype limits remain: 220–660 Hz, volume 0–1, pan -1–1,
  default volume 0.5 and center pan, and 10 ms live control smoothing.
- Each note captures its envelope settings. Retrigger preserves phase and
  attacks from the current level over the full new attack duration.
- Mix normalized stereo samples with fixed 1/9 gain, regardless of active
  voice count. There is no clipping, normalization by active count, or quiet
  diagnostic gain in the bank. SDL applies separate diagnostic gain: approximately 0.791 for the bank, 0.05 for
  the legacy triangle.
- Zero initialization is silent. Reset cancels all voices and restores defaults;
  it is cleanup, not an audible fade. Caller owns storage and synchronization.
- Invalid requests fail without mutation. Valid idle note-off/stop succeed
  harmlessly. Render replaces the caller's interleaved stereo buffer, uses no
  allocation, and accepts zero frames with a null output pointer.

## Automated checks

`ctest --test-dir build -R '^audio_bank$' --output-on-failure` checks silence,
invalid arguments and unchanged state, all nine slots, independent controls,
additive mixing without gain changes, note-off/stop isolation, reset defaults,
and identical results across render block sizes. Nine coherent full-level
voices are checked at left, center, and right against the analytical waveform
and ±1 output bound. This also detects accidental diagnostic attenuation.
The host audio test checks that existing SDL triangle output remains quiet.

The bank can be built without SDL or Lua host support:

```sh
cmake -S . -B /tmp/sugarbee-bank-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-bank-standalone --target test_audio_bank
ctest --test-dir /tmp/sugarbee-bank-standalone -R '^audio_bank$' --output-on-failure
```

## SDL integration and listening

The SDL callback renders the bank at 48 kHz float stereo. Controls use the
existing device lock; the callback allocates nothing and calls no Lua. Sine,
legacy single-triangle, and bank playback are mutually exclusive: note-on is
rejected while an incompatible diagnostic is active, including release/stop.
Within the bank all nine slots may play together. The bank retains fixed 1/9
headroom and SDL then applies separate approximately 0.791 diagnostic attenuation. Normal host
startup remains silent. Device-unavailable fallback and teardown are unchanged.
Native controls are diagnostic-only; cartridge audio ownership/reset and Lua
integration remain future work.

Run from the repository root:

```sh
./build/sbee-bank-test
```

Use a low output volume and stereo headphones or speakers. On Pi use the
preferred HDMI output; see [output troubleshooting](audio-tone.md#pi-troubleshooting-no-audible-tone).
The roughly four-second sequence announces:

1. One quiet left voice (fixed headroom deliberately remains in effect).
2. Three voices across left, center, and right.
3. All nine voices at different pitches and stereo positions.
4. Odd voices release while even voices continue.
5. Remaining voices short-stop, followed by silence and normal exit.

Listen for clean output, distinct additions/releases, correct stereo placement,
and no obvious clicks or distortion. The combined sound should grow with more
voices; an existing voice should not be turned down automatically. Report any
unexpected noise or stuck notes. Also rerun the earlier sine, triangle, and pan
tools, and verify Pong/Save Lab gameplay and normal host exit.

`audio_bank_sdl` checks the real callback against pure-bank output with separate
approximately 0.791 attenuation, one/three/nine active voices, controls, invalid requests,
diagnostic exclusion, release/stop isolation, silence, and active teardown.
`audio_bank_output` runs the listening sequence with SDL dummy output under a
10-second timeout. Passing it verifies control flow, not audible quality.
The user confirmed clean desktop listening at the final gain setting. Pi
validation passed as recorded below. This triangle-only bound does not establish headroom for future resonant filters
or mixed WAV playback.

The first desktop listening run sounded clean but was almost inaudible even at
maximum system volume. Bank diagnostic gain was raised from 0.05 to 0.25 (five
times the amplitude, approximately +14 dB); synth headroom and voice volumes
were unchanged; this intermediate setting remained too quiet.

The 0.25 setting remained a little too quiet in the user's environment. At the
user's request it was raised another 10 dB to `0.25 * 10^(10/20)`, approximately
0.790569415. Fixed 1/9 headroom and legacy diagnostic levels remain unchanged.
Lower system volume before retrying, then adjust to a comfortable level.

The user confirmed that the final level was suitable and the sequence sounded
great. All 50 automated tests pass at this setting.

## Pi validation and output observations

On Pi commit `d71e42d`, the user confirmed all 50 automated tests, clean bank
and earlier diagnostic tones, successful gameplay checks, and Exit to system
status 0. This validates the current native bank checkpoint on desktop and Pi.

Bluetooth audio worked well in an initial test. Other Bluetooth connections
remain untested; this is not a general compatibility or latency guarantee.
HDMI worked well when booting with the cable connected to Pi HDMI port 0 and
a valid display input. The user observed loss of negotiated audio after a port
change. For repeatable validation, connect the intended display/input before
booting and keep that connection stable. Live output switching and reconnection
recovery have not been validated. HDMI remains the preferred Pi test output;
these observations do not add an audio-device switching feature to this scope.

## Noise extension

Bank slots now also support noise through a separate native note-on call. See
[noise seed/retrigger rules and listening checks](audio-noise.md#native-bank-integration).
The original triangle listening sequence remains unchanged. The current bound
also holds for mixed triangle/noise voices; filtering and WAV mixing are deferred.

## Saw extension

Native slots now also support the bounded PolyBLEP saw, with idle-only type
changes. See [saw lifecycle and listening checks](audio-saw.md#native-bank-and-sdl-integration).
All three source types share envelope/control behavior and fixed 1/9 headroom.

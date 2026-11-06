# Cartridge WAV playback (experimental v0.1)

`sugarbee.audio` plays bounded, one-shot PCM WAV assets from the current cartridge.
It is separate from `sugarbee.synth`; both share the existing pause/failure/exit
lifecycle and fixed mixing headroom. No loops, streaming, or automatic device
reconnection are included.

```lua
local sound
local last
function init()
    local reason
    sound, reason = sugarbee.audio.load("assets/chime.wav")
    if not sound then error("Cannot load sound: " .. reason) end
end
function update(dt)
    if sugarbee.btnp("a") then
        local playback, reason = sugarbee.audio.play(sound, {volume = 0.8, pan = 0})
        if playback then last = playback end
        -- voices_full and audio_unavailable are expected refusals; keep playing.
    end
    if sugarbee.btnp("b") and last then sugarbee.audio.stop(last) end
end
```

## Calls

| Call | Success | Expected refusal |
| --- | --- | --- |
| `load(path)` | Opaque asset handle | `nil, reason` for resource/session refusal |
| `play(sound [, options])` | New opaque playback handle | `nil, reason` |
| `is_playing(playback)` | Boolean | False after playback ends |
| `set_volume(playback, volume)` | True | False if ended, stopping, or failed/closed session |
| `set_pan(playback, pan)` | True | False if ended, stopping, or failed/closed session |
| `stop(playback)` | True when a stop starts | False if already stopping/ended or failed/closed |
| `unload(sound)` | True; invalidates this asset handle | `false, "asset_in_use"` or session refusal |

`play` options default to volume **0.5**, pan **0**, and `loop=false`. Options
must be a table when supplied; nil is not a table. Only volume, pan, and
`loop=false` are accepted. True looping is explicitly rejected. Only raw table
entries are read; no inherited options or metamethods run. Values are copied.

All numeric controls require actual finite numbers: volume 0–1, pan −1–1.
Numeric strings, NaN/infinity, extra/missing arguments, unknown fields, wrong
handle types, unloaded asset handles, and invalid values raise descriptive Lua
errors. Invalid controls remain errors even when playback has ended or audio
is unavailable. An uncaught error follows normal Guru handling.

Asset and playback handles are distinct protected userdata, valid only in their
own cartridge session. They cannot be used as each other. Finished playback
handles remain queryable; old handles never control newly reused playback slots.
After successful unload, further play/unload calls with that asset are errors.

## Loading and bounds

Load uses the existing cartridge asset reader: relative ZIP member paths, CRC
verification, and existing archive/entry/path bounds. No OS paths or unrestricted
file access are exposed. Embedded NUL, absolute paths and traversal are rejected.
Loading is supported at top level, in `init`, gameplay callbacks and eligible
`shutdown`, though it is synchronous: prepare sounds in `init` rather than
loading on every collision. Missing, malformed, unsupported, oversized, damaged,
or unreadable assets raise Lua errors.

Supported WAV: signed 16-bit PCM, mono/stereo, 22,050/44,100/48,000 Hz. Conversion
happens once at load time to 48 kHz float. Limits are 16 MiB source, 4 MiB decoded
per sound, 32 MiB retained decoded samples, 64 assets, and eight playbacks.
Repeated play calls share the loaded sound. Repeated loads create separate
assets and consume the budget again; retain your handle instead of reloading.

Resource refusals are `nil, "assets_full"`, `nil, "audio_budget"`,
`nil, "out_of_memory"`, or `nil, "handles_exhausted"`. A failed/closed session
returns `nil, "audio_rejected"` from load/play and `false, "audio_rejected"`
from unload. These are native resource results; Lua allocator failure raises a
Lua memory error. Asset-reader I/O/allocation failures use its existing error
path. Full decode validation precedes budget refusal.

When audio could not open, `load` still validates, converts, and retains samples
under the same budgets; unload works normally. Valid play requests return
`nil, "audio_unavailable"`. There is no silent fake playback handle. Invalid
arguments still raise errors. Normal gameplay should continue.

Native assets are explicitly owned by the session, not Lua garbage collection.
Losing your last Lua reference does not unload a sound. Unload deliberately when
unused; session teardown releases everything, including forgotten references.
Lua handle storage is allocated before acquiring an asset or playback slot.
Temporary native bytes are released before any Lua error is raised. There are no
audio userdata finalizers calling Lua or doing native cleanup.

The asset reader uses up to 16 MiB plus one terminator byte for the selected
entry, while a decoder may hold up to 4 MiB new output alongside 32 MiB retained
samples. Its ZIP/stdio working memory and fixed handle/pool metadata are extra;
there is no second full archive or conversion copy in the binding.

## Playback lifecycle

Overlapping plays are independent. When eight voices are occupied, play returns
`nil, "voices_full"` without stealing or changing an existing voice. Stopping
voices still occupy slots and keep their asset in use until their fade or file
ends. Stop fades for 10 ms; repeating stop does not prolong it. Natural completion
releases the slot. Unload never interrupts playback.

Mono pan uses equal-power endpoints; stereo uses balance, preserving both
channels at center. Live volume/pan edits use the native 10 ms gain ramp. Start
and natural end are not automatically faded; use samples with clean endpoints.
The existing fixed synth gain and WAV headroom remain unchanged.

Host pause freezes playback after its short fade; resume continues. Guru failure
rejects new acquisition/controls and fades sound out. Eligible shutdown retains
access, including load/play; shutdown while paused remains silent. Teardown
stops voices, frees native and unavailable-output assets, invalidates the binding
context before Lua closing callbacks, and finally destroys that context after
`lua_close`. It never waits for sounds to finish.

See [native WAV details](../../tests/audio-wav.md) and the
[WAV Lab](../../examples/wav-lab/README.md) for validation. The user confirmed samples, overlap, pause/resume, exit and
relaunch on coastline; Pi validation of this Lua checkpoint is pending.

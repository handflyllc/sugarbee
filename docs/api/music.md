# Music assets and playback (experimental)

Cartridges have `sugarbee.music.load`, `info`, `unload`, `play`, `state`, `stop`,
and `set_volume`. Loading does not reserve voices or start a timeline.
The existing synth and WAV APIs remain available unchanged.

```lua
local music = sugarbee.music
local song
function init()
    local reason
    song, reason = music.load('assets/music.json')
    if not song then
        print('Music load failed: ' .. reason)
        return
    end
    local info = assert(music.info(song))
    print('Tracks: ' .. info.track_count)
    -- This example expects a three-track asset. Map each track explicitly.
    local ok, why = music.play(song, {voices={1,2,3}, volume=.8, loop=false})
    if not ok then print('Playback refused: ' .. why) end
end
function shutdown()
    music.stop()
    -- Host teardown detaches playback, then frees loaded assets.
end
```

| Call | Success | Operational refusal |
| --- | --- | --- |
| `music.load(path)` | One opaque song userdata | `nil, reason` |
| `music.info(song)` | One fresh metadata table | `nil, reason` for unavailable session |
| `music.unload(song)` | `true`, invalidating the handle | `false, reason` |

`load`, `info` and `unload` each take exactly one argument. `path` must be a nonempty string
without embedded NULs, naming a validated relative member of the current cartridge.
Absolute paths, traversal, backslashes and other unsafe names raise Lua errors.
There is no host-file or network fallback. The host supplies the cartridge path;
cartridge code cannot select another archive through this API.

`info` returns exactly these fields, copied from the validated asset:

| Field | Type | Meaning |
| --- | --- | --- |
| `track_count` | Integer | Number of logical tracks |
| `end_frame` | Integer | Declared end on the asset timeline |
| `timeline_rate` | Integer | Currently 48000 frames per second |
| `loop_eligible` | Boolean | All final release tails fit by the loop boundary |

Changing this table does not change the asset. Loop eligibility is not a guarantee
of an inaudible seam. Info remains available while an asset is borrowed/playing.

Wrong types, wrong argument counts, foreign/WAV handles, and unloaded or stale
handles raise Lua errors. A handle belongs to its cartridge context; it cannot
be reused in another context. Successful unload invalidates all Lua aliases to
that userdata. If native playback holds the asset (including pause/held starts
or finishing), unload returns `false, 'music_in_use'` without changing it.

Missing/damaged/oversized archive members return `nil` plus the existing readable
asset error string. Invalid JSON/schema/preparation returns `nil` plus a bounded
`music.load: ...` diagnostic. Loader allocation failure returns `out_of_memory`.
Collection refusal returns `music_capacity` or `music_id_exhausted`. Closed/failed
contexts return `session_closed` / `session_failed`; an unavailable native session
returns `session_unavailable`. These are separate from argument errors. Error
text for archive/schema diagnostics is for people; do not parse that prose.

## Ownership and absent audio

Limits remain eight loaded assets, 1 MiB total prepared allocations, 256 KiB per
asset, and 4,096 events per asset; input/parser limits are unchanged. Failures
publish no handle and preserve existing assets/playback. No automatic eviction.

Loading, metadata and unload work even when the host has no audio device, using
the same bounded loader/collection in a silent per-cartridge context. No timing
or playback simulation runs there. Valid `play` calls return `false, "audio_unavailable"`;
`state()` is `"idle"`, `stop()` succeeds, and `set_volume` returns `false, "music_idle"`.
Invalid arguments and stale handles still raise errors.

Native sessions own device-backed assets; the Lua context owns silent assets.
Explicit unload or session teardown frees storage. Dropping the last Lua reference
does **not** unload an asset or reclaim its collection slot; keep handles until
unload, or let cartridge teardown clear the collection. Handles have protected
metatables and no native-resource `__gc` callback.

A normal `shutdown` can unload assets before host cleanup. Failure refuses further
music mutations (state remains queryable), retains assets until teardown, and uses the existing native
failure lifecycle. Native session teardown detaches playback before freeing;
contexts are marked closed before `lua_close` and destroyed afterward. No Lua call
occurs under the device lock or while temporary read/parser storage needs cleanup.

## Playback controls

| Call | Success | Operational refusal |
| --- | --- | --- |
| `music.play(song, options)` | `true` | `false, reason` |
| `music.state()` | `"idle"`, `"playing"`, or `"finishing"` | None |
| `music.stop()` | `true`, including idle | `false, reason` for unusable session |
| `music.set_volume(value)` | `true` | `false, "music_idle"` or session reason |

Exact argument counts apply. `options` is required and accepts only:

- `voices`: required dense array, one entry per logical track, containing distinct
  Lua integers in 1..9. `{3,1,2}` maps track 1 to physical voice 3, track 2 to
  voice 1, and track 3 to voice 2. The map is copied. No automatic allocation.
- `loop`: optional Boolean, default `false`. An ineligible asset returns
  `false, "loop_ineligible"`. Selection applies to this start only.
- `volume`: optional finite number in 0..1, default `1`, applied before the first
  sample. Zero starts muted but still advances the timeline.

Options and mapping use raw table entries; inherited fields are not consulted.
Unknown fields, incomplete/duplicate maps, wrong types, non-finite or out-of-range
values raise Lua errors. No voices or playback change on refusal. The one player
must be idle (`music_busy` otherwise), and assigned voices must be inactive
(`voice_busy` otherwise); no stealing. With a valid handle/map, the device-backed
start checks player busy, loop eligibility, then voice availability. An absent
device refuses playback without simulating eligibility or voice contention.

Music reserves its assigned voices through pause, authored rests and finishing.
Direct synth commands cannot change reserved slots; unreserved synth effects and
WAV playback remain independent. Unload refuses a borrowed song until idle.

`stop()` cancels scheduling/looping and uses the existing 480-frame (10 ms) fade.
It never resumes the stopped song, and repeated calls do not extend the fade.
`finishing` includes stop fades and natural release tails. Cleanup restores silent
defaults and releases reservations; if no active voices remain, stop is immediately
idle. A new `play` always starts at frame zero; there is no queue or restart command.

`set_volume(value)` changes only music, ramps over 480 frames, and works while
playing/finishing. Repeating the same target does not restart its ramp. It does
not store an idle preference; the next play uses its own volume or default 1.
Loop boundaries preserve music volume while resetting the assigned bank slots.

Host pause is separate from these three states. It fades the common output then
freezes the song; resume continues it. A start issued while paused is held at
frame zero until resume. Stopping a held/fully paused song cancels it silently.
Ordinary cartridge update callbacks do not run while the host is paused.

See [playback validation](../../tests/music-playback.md),
[Music Lab](../../examples/music-lab/README.md),
[asset-access validation](../../tests/music-lua.md),
[loading rules](../specifications/music-loading.md), and
[native ownership](../../tests/music-assets.md). Composer authoring/export and
expanded preview remain separate checkpoints.

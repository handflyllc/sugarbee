# Music assets (experimental, load/inspect/unload checkpoint)

Cartridges now have `sugarbee.music.load`, `info`, and `unload`. Playback controls
are **not exposed to Lua yet**. Loading does not reserve voices or start a timeline.
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
end
function shutdown()
    if song then music.unload(song) end
end
```

| Call | Success | Operational refusal |
| --- | --- | --- |
| `music.load(path)` | One opaque song userdata | `nil, reason` |
| `music.info(song)` | One fresh metadata table | `nil, reason` for unavailable session |
| `music.unload(song)` | `true`, invalidating the handle | `false, reason` |

Each function takes exactly one argument. `path` must be a nonempty string
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
or playback simulation runs there. Future playback controls will handle unavailable
audio separately; they are not part of this checkpoint.

Native sessions own device-backed assets; the Lua context owns silent assets.
Explicit unload or session teardown frees storage. Dropping the last Lua reference
does **not** unload an asset or reclaim its collection slot; keep handles until
unload, or let cartridge teardown clear the collection. Handles have protected
metatables and no native-resource `__gc` callback.

A normal `shutdown` can unload assets before host cleanup. Failure refuses further
music operations, retains assets until teardown, and uses the existing native
failure lifecycle. Native session teardown detaches playback before freeing;
contexts are marked closed before `lua_close` and destroyed afterward. No Lua call
occurs under the device lock or while temporary read/parser storage needs cleanup.

See [validation](../../tests/music-lua.md), [loading rules](../specifications/music-loading.md),
and [native ownership](../../tests/music-assets.md). Playback/stop/state/volume,
cartridge Music Lab and composer features remain subsequent checkpoints.

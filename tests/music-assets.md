# Native music asset ownership checkpoint

2026-11-09: implemented and accepted on desktop and Pi at `d142156`.
This adds an owned native collection and production-session handle controls.
No cartridge file access, Lua bindings, exporter or editor features are added.

## Limits and ownership

- Eight loaded assets per session; 1 MiB total prepared allocations.
- Existing per-asset 256 KiB and 4,096-event limits remain unchanged.
- Actual prepared bytes include the allocation header, instruments and events;
  allocator bookkeeping and fixed collection/player storage are excluded.
  Input and temporary parser budgets remain separate. Load prepares a candidate
  before publication; the collection limit counts successfully loaded assets.
- One active player, shared by borrowed-song start and owned-handle play.
- Full slots, byte overflow, malformed input or allocation failure never evict
  assets, interrupt playback, or publish a handle. No automatic eviction.
- Loaded storage remains owned after natural completion, stop or music reset.
  Unload refuses the currently borrowed asset, including silent rests, held
  starts, host pause and finishing fades. Other loaded assets may be unloaded.
- Session failure cancels playback using the accepted fade, but retains loaded
  storage until session begin/end/destroy. Mutation calls refuse after failure.
  Teardown first detaches the player, then frees assets on the control thread.
  The callback neither allocates nor frees. Device destruction waits for any
  callback before detaching/freeing.

`sb_music_assets` is an internal platform-independent collection in the existing
`sugarbee_music_load` CMake target. Zero-initialize it once. Insert transfers
ownership only on success; reset frees all assets but preserves the ID serial.
Its caller must serialize access, provide the player's live borrowed-song pointer
to unload, and detach that one player before reset. Do not share a collection
among independent players. This C interface is not a security boundary.

Production wrappers in `src/sb_audio_music.h` enforce those ordering rules under
the device lock: `sb_audio_music_load`, `sb_audio_music_play`, and
`sb_audio_music_unload`. Use one serialized host control thread per device.
Parsing occurs outside the audio lock/callback; publication is atomic under the
lock. Input can be released immediately after load returns. Existing native
music stop/reset/gain/state/reservation controls apply to both playback paths.

Handles are nonzero 64-bit serials scoped to one collection/device; they are not
portable between owners. Unload and teardown invalidate handles. Reuse of slots
or sessions on the same owner never revives an old ID. Serial exhaustion refuses
new handles rather than wrapping. Future Lua bindings must also check owner
identity; native callers must supply handles to their original owner.

Load always clears the output handle on refusal. Its optional loader detail is
initialized to argument/no-offset before parsing; after parsing it describes the
loader result only. The returned asset code is authoritative for publication:
loader success can still be followed by capacity or session refusal. Play returns
bad-handle for stale/zero IDs, or refusal for busy voices/player, invalid maps or
ineligible loops; these leave existing playback unchanged.

## Automated validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
./build/test_music_assets
SDL_AUDIODRIVER=dummy ./build/test_music_assets_sdl
ctest --test-dir build --output-on-failure
```

Native tests cover ownership transfer, active-unload refusal, slot capacity,
actual storage accounting, stale IDs across unload/reset, serial exhaustion,
and every allocation-failure position in a representative load. All tracked
allocations are released. On desktop, eight maximum-event assets total 792,576
bytes, below 1 MiB. The independent byte-limit guard is tested with injected
boundary accounting (exact fit and one byte over); this is not a claim that
current eight-slot assets can naturally exceed the aggregate budget.

Production tests run the actual callback with the dummy device suspended:
load failures while music plays, all capacity refusals preserving state,
asynchronous stop, held/paused starts, pause-stop-resume, reset, natural
completion, teardown/restart/destroy, and input independence. Allocator wrappers
verify no malloc/free calls during callback rendering, including failure cleanup.
Existing borrowed-song, synth and WAV regressions remain in the full suite.

Desktop evidence (2026-11-09, subsequently committed as `d142156`): warning-free
Debug build; **108/108 tests passed**, exit 0 (44.23 seconds, four test workers).
**17 targeted ASan/UBSan checks passed**, exit 0, including both new ownership
tests and existing native/production music, bank, session, WAV and Lua synth
regressions. Leak detection was disabled under tracing; allocator fault/cleanup
checks above are separate evidence. Diff/whitespace review passed. The user confirmed desktop listening acceptance
for `sbee-music-session-test --owned`, exit 0, on 2026-11-09 (America/New_York).
Pi acceptance reported by the user on 2026-11-09 (America/New_York) at confirmed
commit `d142156d9679477d3f6a417c3eef273a54a6dd16`: synchronized hash,
**108/108 tests passed**, exit 0; listening checks passed. Pi validation is
complete. Feature remains unmerged. Only acceptance documentation changed for
this report; unchanged tests were not rerun.

## Listening and handoff

```sh
./build/sbee-music-session-test --owned
```

The existing ~14-second production diagnostic now optionally loads its sustained
chord from embedded JSON and plays via owned handles. Follow the same console
markers: chord/effects/WAV coexistence, music mute, pause/stop/resume, held start,
failure silence, fresh session, and paused shutdown. Fresh-session playback also
checks rejection of the old handle before loading again. Default mode remains
the previously accepted borrowed-song diagnostic. CTest runs both with dummy
output; dummy success does not establish listening acceptance. No files are read
or written by the added music fixture.

After desktop listening acceptance, commit/push on the development machine and
validate the exact hash on the Pi: both new tests, full suite, and the owned
listening mode (HDMI0). Report hash, counts, exit status and listening result.
Validation is complete. Next priority: SugarBeats bounded ownership consumer
checks against the tested pin or its verified documentation-only successor.
No composer expansion or additional engine milestone yet.

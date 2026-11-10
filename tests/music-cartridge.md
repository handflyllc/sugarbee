# Native cartridge music loading checkpoint

2026-11-09: implemented; desktop cartridge checks accepted, exit 0. Pi acceptance pending.
This connects the existing cartridge reader, bounded JSON loader, owned asset
collection and production session. It adds no Lua bindings or composer features.

## Contract

`sb_audio_music_load_cartridge(audio, cartridge, path, handle, read_result, detail)`
is an internal host control-thread operation. The host supplies the cartridge
archive filename; `path` is a relative ZIP member name. Neither is supplied from
Lua in this checkpoint. A successful load returns an owned session handle and
never starts playback. Use the existing handle play/stop/gain/unload controls.
No filename extension or special archive directory is required by the engine;
validation uses the bytes. Cartridge format/version and packaging are unchanged.

`sb_asset_read_limited` reuses the existing archive/path/CRC validation and checks
a caller's member-size cap before allocating or decompressing the named member.
Music passes its existing **1 MiB input limit**. The original `sb_asset_read`
continues using **16 MiB**, and a caller cannot raise that cap through the new
function. A zero cap allows only an empty member. All existing archive-wide
limits remain in force. Empty music then fails the JSON loader.

File IO, decompression, parsing and temporary-buffer cleanup occur on the control
thread, outside the audio callback and device lock. Session checks/publication
use the existing device lock. As with in-memory loading, use one serialized
control thread per device; concurrent lifecycle/load calls are unsupported.
All temporary file bytes are freed after the load attempt. Successful playback
is independent of the archive and source JSON thereafter, including archive
removal. Existing asset collection limits and ownership rules apply unchanged.

Errors remain distinguishable without introducing another parser:

- `SB_MUSIC_ASSET_READ_FAILED`: inspect optional `read_result` for path, archive,
  missing member, size, CRC or IO failure.
- `SB_MUSIC_ASSET_LOAD_FAILED`: inspect optional loader `detail` for JSON/schema,
  preparation-limit or allocation failure.
- Other asset codes retain existing session/capacity/publication semantics.

Failure clears the output handle, preserves loaded assets and leaves current
playback untouched. Unavailable/closed/failed sessions refuse before reading.
No fallback to a host asset filename, automatic playback, eviction, or replacement.
Detail outputs are meaningful for their respective failure stage; the returned
asset code is authoritative. No Lua, cartridge registry/context or silent-device
loading behavior is added here.

## Automated validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
SDL_AUDIODRIVER=dummy ./build/test_music_cartridge
ctest --test-dir build --output-on-failure
```

The native integration test generates temporary valid/malformed cartridges and
checks successful non-autoplay loading; path traversal/absolute/backslash refusal;
missing members/archives; duplicate and unsafe ZIP names; damaged CRC; bad headers;
malformed/schema-invalid/empty JSON; exact input limit and one byte over; and
unchanged legacy read capacity. Oversized music is refused before opening its ZIP
member. Fault injection covers each application allocation through file buffer,
parser and preparation, with matching cleanup and unchanged playing/asset state.

It also exercises full collection refusal, active-unload protection, playback
following archive deletion, failure rejection, teardown and stale session handles.
Rendering is checked for no application malloc/free calls. The existing native,
Lua/WAV, asset reader and packaging regressions remain in the full suite.
Temporary native fixtures are removed on success. CTest packs a separate listening
fixture with the real packager and runs it with dummy output when packer is built;
this is not listening acceptance.

Desktop evidence (2026-11-09, working tree based on `af76f8d`): warning-free
Debug build; **111/111 tests passed**, exit 0 (47.92 seconds, four workers).
**18 targeted ASan/UBSan checks passed**, exit 0, including cartridge loading,
ownership, loader/sequencer, production mixer, session, WAV and Lua synth.
Leak detection was disabled under tracing; injected allocation cleanup checks
are separate evidence. Diff/whitespace review passed. The user confirmed desktop cartridge checks passed, exit 0, on 2026-11-09
(America/New_York). Pi acceptance remains pending; no commit or merge has been
made. Only acceptance documentation changed for this report; unchanged tests
were not rerun. SugarBeats code
and the read-only shared passdown were not modified.

## Desktop listening, then Pi

```sh
./build/sbee-pack -i tests/music-cartridge -o build/music-cartridge.vcart -t "Music Loading Check"
./build/sbee-music-session-test --cartridge build/music-cartridge.vcart assets/music.json
```

This fixture contains the same sustained chord as the accepted owned-asset
listening diagnostic. Follow its familiar music/effect/WAV pause, mute, stop,
resume, failure and fresh-session markers. Both initial load and fresh-session
reload now read the named cartridge member. Expect clean transitions and silence
after fades, with effects preserved by music-only controls. The fixture's Lua file
is only packager scaffolding: launching it in SugarBee does not play music yet.
The diagnostic explicitly maps three tracks to voices 1–3; use the supplied fixture
for this check. No claim of support for arbitrary arrangements in this diagnostic.

After desktop acceptance, commit/push on the dev machine, synchronize that exact
hash on the Pi, run the native test and full suite, and repeat this listening
check via HDMI0. Record hash, test count, exits and listening results. Stop here
for validation, then hand back to SugarBeats for bounded consumer/contract review.

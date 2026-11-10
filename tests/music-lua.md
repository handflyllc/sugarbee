# Lua music asset access checkpoint

2026-11-09: load/info/unload implemented; Pi validation pending.
No Lua playback API, audible Music Lab, exporter or composer changes.
See [API contract](../docs/api/music.md).

## Implementation

A separate host-owned `sb_lua_music` context follows the WAV lifecycle: device
and cartridge path borrowed until destruction, silent assets owned locally when
there is no device. Device-backed assets use the existing native collection.
The main host creates/registers the context before cartridge evaluation, marks
it failed with Guru, ends it before closing Lua, and destroys it afterward.
Partial initialization uses the normal teardown path.

Userdata is allocated and assigned its protected metatable before native loading.
Success returns it without further Lua allocation. All failure paths release
native temporaries before constructing Lua results or raising errors. Metadata
is copied under the device lock; Lua builds its table after unlocking. Mutating
metadata cannot affect native storage. Handle checks include userdata type/size,
context identity, nonzero ID and native collection validity. Unload clears the
userdata ID only after native success.

## Checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure -R '^lua_music_'
ctest --test-dir build --output-on-failure
```

The fixture is packed by the real packager and launched by the real host in two
modes: SDL dummy audio and forced unavailable audio. It loads/inspects on init
and unloads on shutdown. Its silent/blank screen is intentional: no playback or
Music Lab UI exists yet. `build/music-lua.vcart` can also be launched normally;
console messages identify successful load/info and unload.

Tests cover metadata values/types/copy independence, return counts and stack
balance; missing/invalid assets; malformed paths/types/arity; protected userdata;
WAV/foreign/unloaded/stale handle rejection; eight-slot capacity and recovery;
Lua allocation failure before load and during info without native mutation;
and unavailable-device parity. Native-only playback checks verify Lua unload
refuses active, paused and finishing assets. Existing Lua music playback names
are asserted absent.

Real host lifecycle coverage includes normal shutdown, repeated launches,
Guru failure after load, init-callback failure and cleanup. A host-installed
finalizer checks calls observe a closed context during `lua_close`; cartridge
`__gc` restrictions remain unchanged. Guru dismissal between test launches follows
the launcher's state transition. Existing WAV/synth and close-limit regressions
remain in the full suite. Allocation-failure tests concern Lua allocation;
previous native loader fault-injection tests remain separate coverage.

Desktop evidence (2026-11-09, working tree based on `8bbc546`): warning-free
Debug build; **114/114 tests passed**, exit 0 (37.25 seconds, four workers).
All **24 selected checks in the ASan/UBSan build passed**, including fixture
setup, both Lua music/device modes, both existing WAV modes, and the native
music/ownership/session regressions. Leak detection was disabled under tracing;
no LeakSanitizer result is claimed. Diff/whitespace review passed. No Pi result
is claimed; implementation is uncommitted/unmerged. SugarBeats code and the
read-only canonical passdown were not modified.

## Pi handoff

This checkpoint changes asset access, not sound output; no new listening claim
or listening gate is introduced. After commit/push on the development machine,
synchronize the exact hash on the Pi, rebuild and run the targeted checks above
and the full suite. Expect both device/unavailable cases to pass with exit 0.
Record hash/count/exit status, then hand the tested pin back to SugarBeats for
bounded consumer/API contract review. Stop before Lua playback implementation.

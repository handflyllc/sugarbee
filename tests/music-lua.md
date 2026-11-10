# Lua music asset access checkpoint

2026-11-09: load/info/unload accepted on desktop and Pi at `4910b38`.
This historical checkpoint added no playback API or audible Music Lab.
The subsequent [playback checkpoint](music-playback.md) extends these tests;
exporter and composer changes remain out of scope.
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
UI is included in this asset fixture. `build/music-lua.vcart` can also be launched normally;
console messages identify successful load/info and unload.

Tests cover metadata values/types/copy independence, return counts and stack
balance; missing/invalid assets; malformed paths/types/arity; protected userdata;
WAV/foreign/unloaded/stale handle rejection; eight-slot capacity and recovery;
Lua allocation failure before load and during info without native mutation;
and unavailable-device parity. Native-only playback checks verify Lua unload
refuses active, paused and finishing assets. Playback names were absent at the original checkpoint; current tests also
exercise the playback contract described in the linked follow-up.

Real host lifecycle coverage includes normal shutdown, repeated launches,
Guru failure after load, init-callback failure and cleanup. A host-installed
finalizer checks calls observe a closed context during `lua_close`; cartridge
`__gc` restrictions remain unchanged. Guru dismissal between test launches follows
the launcher's state transition. Existing WAV/synth and close-limit regressions
remain in the full suite. Allocation-failure tests concern Lua allocation;
previous native loader fault-injection tests remain separate coverage.

Desktop evidence (2026-11-09, subsequently committed as `4910b38`): warning-free
Debug build; **114/114 tests passed**, exit 0 (37.25 seconds, four workers).
All **24 selected checks in the ASan/UBSan build passed**, including fixture
setup, both Lua music/device modes, both existing WAV modes, and the native
music/ownership/session regressions. Leak detection was disabled under tracing;
no LeakSanitizer result is claimed. Diff/whitespace review passed.

Pi acceptance reported by the user on 2026-11-09 at synchronized commit
`4910b38b4b75a77e6b2c8bd86497ea238939d4f9`: **114/114 tests passed**, exit 0.
Engine acceptance is complete; next priority is SugarBeats consumer/API contract
review. No listening claim is added for this silent checkpoint. Feature remains
unmerged. Only acceptance documentation changed; unchanged tests were not rerun.
SugarBeats code and the read-only canonical passdown were not modified.

## Pi handoff

This checkpoint changes asset access, not sound output; no new listening claim
or listening gate is introduced. After commit/push on the development machine,
synchronize the exact hash on the Pi, rebuild and run the targeted checks above
and the full suite. Expect both device/unavailable cases to pass with exit 0.
Record hash/count/exit status, then hand the tested pin back to SugarBeats for
bounded consumer/API contract review. Stop before Lua playback implementation.

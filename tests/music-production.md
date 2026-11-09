# Prepared music in the production SDL session

2026-11-09 checkpoint: implemented; desktop diagnostics, Musical Synth Lab and
WAV Lab lifecycle accepted by the user. Pi acceptance remains pending.
No cartridge music loading, registry, Lua music API, or stable native SDK is added.
The existing Lua synth and WAV APIs retain their signatures and behavior when
no music owns their voices.

## Contract

`src/sb_audio_music.h` provides internal, device-locked controls for already
prepared songs: start (explicit map and loop flag), stop, reset, music gain,
state and reservation queries. Start is allowed only in a running cartridge
session. It validates atomically and refuses busy/invalid maps without stealing.
All direct synth mutations, including atomic settings application, refuse
reserved voices. Read-only active queries remain available for sounding voices
in a running session. Music state/reservation queries also work after failure.

Song storage and its immutable arrays are borrowed until idle, music reset,
session begin/end or device destruction. Stop is asynchronous unless held or
fully paused; calling stop alone does not permit freeing an active song.
Failed-session controls refuse mutation; wait for idle or end the session before
freeing borrowed storage. Device close waits for the callback before destruction.
Reset is immediate music-only cleanup and may click if used audibly; use stop
for an audible ending.

The backend owns one `sb_music_shared` instance. Each active frame renders its
synth bank once, adds the existing WAV mix, then applies **one** host fade.
The original synth output scale and independent WAV headroom remain unchanged.
Music gain affects only reserved synth voices. The common native transport now
has an internal frame split so SDL can insert WAV before applying the host fade;
private/shared offline rendering uses the same frame operations. Do not call
both the split protocol and the complete shared renderer for the same frame.

Pause fades all sources over 480 frames, then freezes music, effects, WAV
positions and gain ramps. Resume/reversal and held-start behavior follow the
[combined shared-owner contract](music-shared-pause.md). Held starts during
fade-out skip only music voices; effects and WAV continue until host silence.
Music stop/reset/natural completion preserve effects, WAV and host pause state.
Failure cancels music scheduling immediately and fades the whole mix to silence;
it clears reservations, borrowed song and all sounding sources by fade completion.
Already silent failure clears immediately. WAV asset memory remains owned until
session teardown; no allocation or freeing occurs in the callback. Exit, fresh
session and destroy clear the music borrow without waiting for playback.

## Automated checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
SDL_AUDIODRIVER=dummy ./build/test_music_sdl
ctest --test-dir build --output-on-failure
```

The production callback regression compares music + effect + WAV samples against
independent sequence/WAV rendering with an analytic host fade. It checks exact
fade-in/out output, full state freeze, whole/split parity across loop boundaries,
music gain, every reserved synth mutation, atomic refusal, held starts with
continuing effects/WAV, stop during pause, music-only reset, natural completion,
failure, paused shutdown, fresh sessions, and destruction while borrowing.
Existing native, Lua synth, WAV and session tests remain regression coverage.
The listening tool also runs with SDL dummy output in CTest; this is functional
coverage, not audible acceptance.

Desktop evidence (2026-11-09, uncommitted working tree based on `4292b88`):
warning-free Debug build; **105/105 tests passed**, exit 0 (120.20 seconds).
**15 targeted ASan/UBSan checks passed**, including production SDL mix, native
private/shared pause, loader/loop/gain/stop, bank, session, WAV and Lua synth.
Leak detection was disabled under tracing; no leak-check result is claimed.
Diff/whitespace review passed. On 2026-11-09 the user reported all three desktop
diagnostics passed (`sbee-music-session-test`, `sbee-session-test`,
`sbee-wav-test`), along with pause/exit/resume/reload in Musical Synth Lab
(Eine kleine Nachtmusik). The user also confirmed WAV Lab cartridge lifecycle
checks passed on 2026-11-09;
Pi acceptance remains pending. No engine code changed for this record update,
and unchanged automated checks were not rerun.
SugarBeats code and canonical passdown were not modified (read-only
sibling location); the user must set the passdown to Waiting for validation.

## Desktop listening, then Pi

Start at low speaker volume:

```sh
./build/sbee-music-session-test
./build/sbee-session-test
./build/sbee-wav-test
```

The new ~14-second diagnostic uses a sustained triangle chord, an independent
left-side saw effect and a right-side high sine WAV. Follow console markers:
all sources pause/resume together; music mute leaves both effects audible;
stopping music during pause does not resurrect it; starting paused stays silent
until resume; failure silences everything; a fresh session has the chord alone;
paused shutdown stays silent. Expect no unexpected blips after fades or pauses.
The sustained fixture avoids authored note onsets near wall-clock controls.
The tool uses the production backend, unlike `sbee-music-test`'s private host.
No WAV capture files are created.

Also run an existing synth cartridge and WAV Lab through pause/resume, exit and
relaunch; confirm existing sounds still behave normally and Exit to system is 0.
After desktop acceptance, commit/push on the development machine. On the Pi,
synchronize that exact commit, rebuild, run the full suite and the same listening
and cartridge checks (HDMI0 is the user's available validation path). Record
hash, test count, exit statuses and listening observations. No Pi result is
inferred from desktop or SDL dummy checks.

Stop here for validation, then hand one tested pin back to SugarBeats. The
bounded follower task is rebuild/regression plus review of the production
contract; it does not authorize new preview/editor layers or Lua integration.

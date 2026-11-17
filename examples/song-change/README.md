# Song-change example

Two one-second triangle tones demonstrate a cartridge-owned song-change policy.
A is 440 Hz; B is 660 Hz. Both use voice 1. This example does not change the
engine API or add native queueing, seeking, crossfading or voice stealing.

```sh
cmake --build build -j4
./build/sbee-pack -i examples/song-change -o build/song-change.vcart -t "Song Change"
./build/sugarbee build/song-change.vcart
```

| Button / keyboard | Action |
| --- | --- |
| A / J | Start or restart A |
| B / K | Start or restart B |
| X / U | Stop and cancel pending playback |
| Start / Enter | Existing host pause menu |

## Policy to reuse

`request_song(name)` and `song_change` are ordinary cartridge functions/data,
not SugarBee APIs. The example keeps them global for reuse and deterministic
tests; the song handles and update helper remain local. Copy/adapt this small
pattern within a game's `main.lua`; no module loader is required.

1. Validate that the requested song loaded successfully. A missing/unknown song
   reports an error without disrupting existing playback or the pending request.
2. If the player is not idle, request a stop. A stop refusal preserves existing
   bookkeeping. After a successful stop, clear `current` immediately.
3. Replace `pending` with the latest request. A nil request cancels it. Requests
   during finishing may repeat stop; the engine does not extend that fade.
4. In `update`, wait for idle before playing the pending song. Only a successful
   play sets `current`. This includes asking for the same song during its fade.
5. On `music_busy`, retain the request and retry after idle. Other refusals are
   displayed and clear the pending request; the player must explicitly request
   playback again. Natural completion clears current without restarting.

This policy deliberately restarts an already-playing song when selected again.
A game's background-music helper may instead suppress redundant requests while
actually playing; it must still distinguish that from a stopped/finishing song.
The last pending request wins. Stop takes priority over B, and B over A, if input
edges arrive together. One owner must control this transport; outside code must
not secretly play or stop another song while relying on this current-song record.

The display distinguishes the current selection from the engine transport:
`current` becomes none immediately on stop even though finishing may still sound.
No audio device is handled with an `audio_unavailable` message. Only voice 1 is
mapped; a synth effect already occupying it produces `voice_busy` rather than
being stolen. Resolve contention and make a fresh request. Successful loading
uses two of the eight music asset slots; session teardown releases both assets.

## Verification and acceptance

Base engine/tooling commit: `247b95f`. No production runtime changes.
Warning-free desktop build and 24 targeted serial CTests passed, including:

- The same policy scenarios against the smoke model and production Lua/native
  music playback: stop/same-song restart during finishing, replacement/cancellation
  of pending requests, natural completion, unavailable audio and clean relaunch.
- Native verification of actual A/B pitch, an active stop still finishing after
  479 frames and idle after 480, real synth voice contention, keyboard controls,
  drawing and asset cleanup. Native metadata checks also verify the smoke scenario.
- Injected busy-play/failed-stop/permanent-refusal policy branches. These are
  explicitly injected, not claims that those races happened spontaneously.
- Smoke-only missing-asset handling and seeded cartridge execution, plus existing
  native music ownership/stop/Lua checks and all prior smoke games.

The configured suite now has 141 tests when all dependencies are present; that
is not a new full-suite result. Dummy audio does not establish listening quality.
The smoke model's finishing interval is synthetic; the 479/480 assertions use
actual native rendering with the SDL callback paused and advanced by the test.

Desktop manual acceptance completed: on 2026-11-17 the user confirmed all
`song-change.vcart` checks below passed against the working changes based on
`247b95f`. This is user-reported listening/UI acceptance; no new Pi result is
claimed.

1. J plays the lower tone; K plays the higher tone. Both finish and show none/idle.
2. Tap J repeatedly or alternate J/K while sounding: the last requested song
   starts, without getting stuck silent. Deterministic tests cover requests
   inside the short fade; manual input need not hit an exact 10 ms window.
3. U stops playback and cancels pending requests. J/K can start again afterward.
4. Pause/resume, exit and relaunch: controls and displayed state remain usable.

No new engine/Pi acceptance cycle is required for this example-only change.
Its live Pi behavior is not newly claimed; it can be auditioned there separately.
The next graphics capability remains a future discussion.

# Native shared-owner host pause checkpoint

The offline `sb_music_shared` owner now includes host pause. Both it and the
private `sb_music_host` adapter use the same `sb_music_transport` implementation.
This combines already accepted ownership and pause semantics without connecting
production SDL, WAV, Lua or cartridge playback.

## Contract

- Initialize with `sb_music_shared_init`: silent idle, unpaused, host gain 1.
  One shared renderer advances all nine physical voices; do not wrap it in the
  private host adapter or separately render its bank.
- `sb_music_shared_pause(owner, true)` fades the entire music/effect mix over
  480 frames, then freezes it. Song/event position, synth/filter/noise/envelope
  state and music gain ramps stop advancing once fully silent. Rendering emits
  exact zeros while fully paused.
- Resume continues both music and effects from frozen state through a 480-frame
  fade-in. Reversals ramp from current gain; repeated requests do not extend the
  fade. Music gain is separate and still affects only music-owned voices.
- A music start while pause is requested reserves/configures voices atomically,
  reports playing, and holds opening events and gain at frame zero until resume.
  During an unfinished host fade, already sounding effects must keep rendering;
  reserved music slots are skipped entirely, not rendered a second time or
  advanced silently. Once host silence is reached, effects also freeze.
- Stopping fully paused or held music resets only its reserved slots, releases
  reservations and drops the borrowed song reference immediately. Effects and
  host pause state remain intact. During pause fade, stop cancels music events
  immediately and cleans up no later than host silence. Resume never cancels a
  pending stop. Natural completion likewise cleans up only music-owned slots.
- `sb_music_shared_reset` is immediate music-only cleanup: preserve effects,
  paused state, current host fade and its remaining frames; clear held-start and
  pending-stop state. Whole-owner `init` is initialization/teardown, not music stop.
- Existing guarded direct controls remain allowed on unreserved voices while
  paused. Controls can change settings/targets, but rendered state does not
  advance until resume. Controls targeting reserved voices still refuse unchanged.
- Existing atomic start refusal, gain range/defaults, asset lifetime, zero-frame
  and invalid-buffer behavior remain. All controls and rendering are serialized
  by the caller; assets outlive playback until idle or music reset.

## Compatibility and implementation

Private host API behavior is preserved; both owners delegate lifecycle operations
to the common native transport. Internal host fields now live under `transport`.
The shared owner grows from 7392 to 7432 bytes on desktop; bank and sequencer
layouts are unchanged in this checkpoint. Rebuild consumers and update any
internal field inspection. No stable ABI is promised. CMake's sequence target
includes `sb_music_transport.c`; manual source lists must include it too.

The bank's render loop can skip reserved slots while held-start music waits.
It advances unreserved effects once per frame, including filter and waveform
transition state. Rendering still performs no allocation, parsing, SDL, Lua or I/O.

## Validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
./build/test_music_shared_pause
ctest --test-dir build --output-on-failure
```

The combined test compares the full music/effect output against an unpaused
shared reference multiplied by an independent host gain schedule. It checks
exact whole/single-frame equality; whole-state freeze; gain changes and direct
controls while paused; both reversal directions; atomic refusal; held starts
during fade and full pause; effect progression during a held start; paused stop;
stop/resume races under serialized controls; music reset preserving host fade;
and natural music completion while effects continue. Exact effect state checks
protect against double rendering and unintended reset.

Desktop Debug build: warning-free. Full suite **103/103 passed** (107.56 s).
All 19 focused music/bank/session checks passed; ten native music/bank ASan/UBSan
checks passed with leak detection disabled under tracing. Diff/whitespace review
passed. Pi native validation remains pending.
This offline checkpoint introduces no new listening mode or claim of audible
shared playback. Existing private-host SDL diagnostics remain regression checks.

After engine acceptance, return to SugarBeats for bounded offline combined
host/shared ownership checks. Production SDL routing, WAV coexistence, Lua and
cartridge integration remain deferred.

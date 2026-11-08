# Prepared native music sequencing checkpoint

2026-11-07, based on RC1 `cca7c62`. Experimental internal interface; no new
cartridge API. This checkpoint needs desktop listening and Pi acceptance before
advancing to music loading or production engine integration.

## Implemented scope

`sugarbee_music_sequence` links the existing pure-C synth. Its internal header is
`src/sb_music_sequence.h`. A sequence owns an exclusive private bank: track N
uses slot N. This is a fixture/preview core, not yet a shared-voice game player.
The normal host, synth API, WAV path, and cartridge format remain unchanged.

A song contains fixed instruments and a caller-owned, immutable ordered event
array. Keep the song and its arrays alive until idle or explicit reset. Start
validates/configures a temporary bank outside rendering and publishes only on
success; busy or invalid start leaves the previous state intact. All operations
must run on one thread or under the host's synchronization. No allocation, SDL,
Lua, file parsing, logging, locks, or internal threads occur in the core.

Timing uses absolute uint64 frame positions at 48 kHz. Events sort by frame,
off-before-on, then track; duplicates of the same type/track/frame reject.
Matched on/off pairs must have positive holds; held notes cannot overlap per
track. Release-tail retriggers are allowed. All events at a position apply before
that audio sample, so a same-frame off/on adds no artificial gap. End-frame
events are dispatched upon reaching the boundary even when a render buffer
ends there. Zero-frame renders dispatch nothing. Natural tails continue after
the declared end; when finished, the private bank resets to silent defaults and
the borrowed song pointer is released. Position/state fields are internal and
must not be edited by consumers.

Prototype limits: 1–9 tracks, at most 4096 events, positive end position.
The event cap bounds setup work and is NOT a settled JSON resource budget.
Empty event arrays and unused tracks are permitted only as native fixture
conveniences; runtime asset policy remains pending. Non-pulse width is ignored.
Noise/off event frequency must be zero. Pitched on is finite 110–880 Hz.
Existing bank functions validate instrument ranges. Ordinary C pointer validity
and buffer capacity/non-aliasing remain caller obligations.

Reset cancels immediately and is only cleanup. There is no audible stop API,
looping, reservation/mapping, music gain, pause control, loader, or Lua binding
in this checkpoint. The future game integration must implement those accepted
behaviors separately. Sample-accurate scheduling does not eliminate device
latency or establish bit-identical floating-point output across architectures.

## Automated checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

`music_sequence` compares samples against independently scheduled direct bank
calls, covers simultaneous mixed chords, changed pitch, same-frame off/on,
release tails and buffer-end events, noise, bounds, silent defaults, rejection
atomicity, malformed sequences and buffer-partition invariance.
`music_sequence_sdl` runs a short prepared song through an actual SDL callback
using the dummy driver. Its fixture storage remains alive until callback use
ends. Dummy output proves dispatch/cleanup, not sound quality or device latency.

## Desktop listening gate

Start with low speaker/headphone volume, then run:

```sh
./build/sbee-music-test
```

The roughly 30-second sequence labels each section in the terminal:

1. Triangle unison, octave, fifth, major third, then major/minor triads.
2. Saw major/minor chords, repeated chords, and adjacent chord changes.
3. The same sequence using pulse, then mixed triangle/saw/pulse.
4. A short original melody over two accompanying voices.

Listen for tuning, distinguishable intervals/chords, balanced harmony,
simultaneous attacks, intelligible melody and clean final silence/exit. Note
specific sections that sound harsh, indistinct, unexpectedly slurred, or click.
Some beating and brighter timbres are expected; reports should guide diagnosis,
not be dismissed as unavoidable or treated automatically as oscillator defects.
The tool uses fixed bank 1/9 headroom plus the current SDL bank attenuation
0.790569415; there is no automatic normalization between waveform types.

Rerun to check silent exit/relaunch. Run the existing synth/musical/WAV labs and
Pong/Pac-Man if checking interactive regressions. Production host code has not
been connected to this sequencer.

## Pi acceptance and project handoff

After the development checkpoint has a transferable commit, fetch the exact
reviewed commit on the Pi, build/run the suite and the same listening executable.
Record the hash, test counts, listening observations and exit status. Do not
claim Pi acceptance from desktop or dummy-driver checks.

The next follower is SugarBeats: after desktop/Pi acceptance, update its pin and
exercise prepared fixtures through this core before building further engine
layers. Its own preview-host work can proceed using existing bank controls.
Neither side should build several layers on unverified behavior. Explicitly
tell the user when switching projects is the next useful step.

## Validation record

- Debug build completed without compiler warnings; both new tests pass.
- AddressSanitizer/UndefinedBehaviorSanitizer native checks pass with leak
  detection disabled. LeakSanitizer cannot run under this environment's tracing;
  no leak-sanitizer result is claimed.
- Full Debug suite: 91/91 passed. Complete five-section diagnostic passed with
  SDL dummy output; exit status 0. These are not listening results.
- Desktop validation/listening passed per the user on 2026-11-07, against
  the uncommitted candidate based on RC1 `cca7c62`. Pi was not tested: hardware
  is unavailable today. Queue the full suite and listening procedure above for
  the next Pi validation batch, recording the exact transferred commit.
- Pi acceptance remains a gate for shared-sequencer adoption and further engine
  layers. SugarBeats bank-only Phase 0B may proceed independently against its
  reviewed clean baseline; do not bypass its source-pin/clean-tree guard.
- Working changes are now on `feature/native-music-sequencer`, uncommitted.
  Earlier branch creation failed because `.git` was read-only in this environment;
  the branch now exists. No candidate commit or Pi acceptance is claimed.

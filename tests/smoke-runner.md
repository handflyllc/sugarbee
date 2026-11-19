# Cartridge smoke-runner services

Tooling checkpoint based on `6d50b23`. No engine runtime, public API, asset format
or SugarBeats dependency changes. R3's missing music/save execution coverage is
addressed; the runner remains a partial simulation, not a second engine.

## Run

```sh
lua5.4 tests/cartridge_smoke.lua examples/save-lab 3000 1
lua5.4 tests/cartridge_smoke.lua examples/music-lab 3000 1 tests/smoke-runner/music-lab.lua
```

Existing three-argument invocations still work. The optional fourth argument is
a **trusted test-side Lua scenario** returning a table. It is not loaded from the
cartridge automatically and is not a new cartridge format. For your own game:

```lua
return {
    songs = {
        ['assets/music.json'] = {
            track_count = 3,
            end_frame = 192000,
            loop_eligible = true,
        },
    },
    audio_available = true, -- optional; false exercises unavailable playback
}
```

Use metadata from a native-validated asset and update the scenario when that
asset changes. The runner checks safe relative names and file existence, but
**does not parse or validate the music JSON**. Missing files return nil plus an
asset error; existing files without scenario metadata fail with an explicit
harness diagnostic. Track count, end frame and loop eligibility are required;
there are no invented defaults. The model reports timeline rate 48000. Scenarios
can deliberately supply synthetic metadata for logic tests, as the small runner
lifecycle fixture does. Passing those tests says nothing about JSON correctness.

## Save behavior

One in-memory string per invocation, initially absent. No player save files,
UUID storage directories, HOME/XDG paths or persistence across invocations are
accessed by these stubs. Empty strings and embedded zero bytes round-trip.
`save`/`load_save` check argument counts and types and return exactly two results.
More than 65536 bytes returns `nil, 'too_large'` without replacing the old value;
missing data returns `nil, 'not_found'`. Valid calls during top-level loading
return `nil, 'io_error'`; init/update/draw/shutdown can use the memory slot.

Disk errors, durability, filesystem permissions and failure injection are not
modeled. Use the real storage tests for those. The runner and scenario execute
trusted Lua in the ordinary interpreter: this is not a sandbox restricting what
cartridge/scenario code itself can do through `io` or `os`.

## Music behavior

The test-only module `smoke_services.lua` models:

- `load`, `info`, `unload`: distinct opaque table tokens, copied metadata,
  stale/foreign-handle errors, eight loaded slots, in-use unload refusal.
  Native handles are userdata; do not use stub token type as an API guarantee.
- `play`: exact argument count, explicit options, one distinct integer voice
  in 1–9 per declared track, loop/volume validation, `music_busy`,
  `loop_ineligible` and optional `audio_unavailable` refusals.
- `state`, `stop`, `set_volume`: idle/playing/finishing states, repeated stop,
  and `music_idle` when setting idle volume. Gain is validated but no sound or
  smoothing is produced.

Before each update, the harness advances 800 synthetic timeline frames (one
60 Hz step). A nonlooping song reaches `finishing` at its declared end, then
becomes idle at the next advance. Stop enters finishing until the next advance;
repeated stops do not prolong it. Looping songs remain playing until stopped.
This exposes a busy interval for cartridge logic without copying native release
or pause algorithms. It does **not** reproduce the 480-frame stop fade, natural
release lengths, silent immediate stop, sample/event timing, host pause,
voice activity, or synth/music contention. Synth stubs still always succeed;
`voice_busy` and reserved-voice synth refusals are not simulated.

Prepared-memory/parser limits, native collection identities, shutdown/failure
state transitions, real loop eligibility, event scheduling and sound remain the
responsibility of native loader/Lua/transport tests and listening checks. The
song-change example and its real-engine validation are a separate checkpoint.

## Unsupported functionality and other limits

Unknown `sugarbee`, music and synth members, and WAV methods, fail explicitly
with `cartridge_smoke: unsupported API ...`. WAV support is deferred. Optional
feature probing through absent fields therefore also raises a diagnostic here.

The line stub checks arity, native coordinate range and 0..255 color channels,
then counts one drawing call. It does not simulate rasterization or clipping.
Sprite tint/reset stubs validate arity and channels (0..255); no color or alpha
blending is simulated.
Camera and camera-reset stubs validate arity and native integer range only;
they do not simulate coordinate translation.
Existing graphics and input checks are retained. `clip` and `clip_reset` validate
argument counts, native integer bounds and nonnegative dimensions, but do not
simulate clipped pixels. The runner still approximates
binding validation; it does not reproduce all native integer bounds, restricted
Lua libraries, instruction budgets, ZIP checks, image decoding, rendering or
physical input. Success output explicitly says services are simulated. Random
input is seeded, but passing a single run is not exhaustive gameplay coverage.

## Verification

19 targeted serial CTests passed on desktop, including all eight existing
cartridge smoke games, Music Lab, Save Lab, service-model unit checks, a runner
lifecycle fixture, explicit error-message checks, and existing native Lua
music/synth and host save tests. No full native-suite rerun or new Pi runtime
acceptance is needed for this tooling-only change. Five new CTest registrations
bring the configured total to 136 when the Lua interpreter is present.

The model tests cover save boundaries/binary/empty values and per-run isolation;
metadata copying; missing/foreign/stale handles; malformed mappings/options;
busy/unload/loop refusals; natural end and repeated stop; same-asset replay after
finishing; capacity recovery; and unavailable audio. Integration checks exercise
actual runner callback wiring and require both a failing process and the expected
diagnostic for unsupported APIs, absent metadata and unsafe paths.

The `circle` and `circle_fill` stubs validate arity, signed native integer
centers/radii, nonnegative radii and 0..255 RGB channels, and count draw calls.
They do not simulate pixels, clipping or camera transformations; native graphics
tests and manual demo acceptance cover rendering.

The `triangle_fill` stub validates six signed32 coordinates and optional checked
RGB channels, counts the draw call and returns no values. It does not model
pixel-center coverage, shared-edge ownership, camera offsets or clipping.

The runner overrides the interpreter's `require` with a cartridge-directory model
in `smoke_modules.lua`: dotted names, text-only compilation, private cache,
false/nil result handling, cycles/retries and module loading limits. It never
falls back to the interpreter's package search. Inputs are trusted local files;
symlink confinement, ZIP/CRC failures, allocation failures, native compilation
cost and instruction budgets are not modeled. Use native module tests for those
engine contracts. This does not make the runner a security sandbox.

## Canvas metadata

The runner implements zero-argument `sugarbee.width()` / `height()` queries.
Cartridges without configuration default to 1280×720. For a cartridge containing
`console.json`, provide `canvas={width=300,height=200}` in the trusted scenario
(or another supported pair). Missing metadata is an explicit harness error.
This follows the music metadata approach: the runner does not parse JSON, test
CRC/size limits, verify metadata matches the file, render or scale pixels.
Native tests remain authoritative. Synthetic canvas metadata is also permitted.

```sh
lua5.4 tests/cartridge_smoke.lua examples/canvas-demo 1 1 tests/canvas_scenario.lua
```

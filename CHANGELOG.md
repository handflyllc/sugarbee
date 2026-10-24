# Changelog

## Unreleased (SugarBee 0.1 baseline)

- Separated cartridge cleanup from host shutdown. The cartridge-ending
  operation invokes the eligible Lua shutdown callback, closes Lua, releases
  cartridge textures, and clears session flags. Texture cleanup preserves
  host graphics resources and invalidates old handles. Added regression
  checks for repeated cleanup, retained-frame preservation, and texture reuse.
  Exit Game still closes the application; returning to a launcher is pending.

- Added a host-controlled pause menu with Resume and Exit Game, operated by
  any controller or the keyboard. Start is now host-reserved and unavailable
  to cartridge input queries. Pause stops Lua update and draw callbacks,
  retains the game image, and suppresses held menu inputs until released.
  Exit Game follows the normal shutdown callback and cleanup path.
  Added retained-frame pixel checks to the graphics regression test.

- Replaced the unattributed template font with a pinned, upstream-declared
  public-domain font8x8 table, preserving attribution and documenting provenance.
  Added lowercase and remaining printable ASCII glyphs; retained 8x8 metrics.
  Removed the old bitmap copy from the historical template.

- Added Guru Meditation message wrapping at spaces, splitting of long words,
  and a continuation notice when message storage or screen height is exceeded.
  Complete supplied diagnostics are written to standard error before the
  bounded screen copy is made. Added a manual three-case regression cartridge.

- Added optional Lua `shutdown()` invocation on normal application exit
  after successful cartridge initialization. The callback runs before
  native teardown and is skipped while Guru Meditation is active.
  Shutdown errors are reported to the console without preventing cleanup.
  Added a manual five-case regression cartridge and lifecycle documentation.

- Milestone 3C: Added source-region rendering, destination scaling with
  nearest-neighbor filtering, horizontal/vertical flips, and the Lua
  `sprite_region()` binding. Included rendered-pixel assertions and a
  four-frame animated PNG sprite-sheet cartridge example and atlas PNG test.

- Added bounded, language-neutral `.vcart` resource reads and PNG-to-RGBA
  decoding, plus Lua `texture_load()`, a cartridge PNG example, asset/decoder
  tests, and matching build/API documentation. The game package format is
  unchanged; the host now requires libpng development/runtime support.


- Added an experimental language-independent RGBA8 texture handle API and SDL2
  sprite drawing path, with Lua `texture_rgba`, `sprite`, and `texture_free`.
  Added a procedural bee sprite example, headless texture checks, and updated
  graphics/build/architecture documentation. Cartridge asset decoding is 3B.

- Added an SDL-independent, 60 Hz fixed-step C timing scheduler with a bounded
  catch-up budget and `update(dt)` Lua callback (old `update()` remains valid).
- Updated input smoke tests to tolerate already-connected SDL gamepads, while
  verifying virtual controller hotplug relative to the initial device count.
- Buffered logical input press edges across zero/multiple simulation updates,
  including a press-and-release drained in one SDL event poll. Updated
  moving-box to move in pixels/second, added timing tests, and synchronized
  input/timing documentation.

- Added SDL2 gamepad discovery/hotplug handling behind a language-independent
  `sb_input.h` interface, plus developer keyboard fallback for player zero.
- Kept Lua `sugarbee.btn()` and added `sugarbee.btnp()` for pressed edges;
  added headless input smoke tests, a moving-box demo, and input documentation.

- Extracted Lua-independent 2D drawing services into `sb_graphics.h` and an
  SDL2 backend; preserved existing `cls`, `rect`, `print`, and Guru rendering.
- Added a headless graphics smoke test and synchronized graphics/architecture
  documentation. Input and timing were not part of that earlier graphics change.

- Added CMake build and CTest baseline for the current C host and packager.
- Restored bitmap-font data from the historical template (partial ASCII font).
- Added explicit little-endian cartridge header encoding/decoding and full
  minimum version validation (major, minor, patch).
- Reworked the Linux packager to create ZIPs without invoking a shell and
  added random UUIDv4 generation plus `--uuid` reuse for game save identity.
- Added build, dependency, and cartridge-format documentation.

Not a release: Lua sandboxing, analog input, and complete script execution
limits remain outstanding.

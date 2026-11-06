# Building SugarBee (Debian 12)

Install the packages in [dependencies.md](dependencies.md). Run from the
repository root. **Do not build as root**.

## Configure and compile

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected executables (when dependencies are available):

- `build/sugarbee` — SDL2/Lua host prototype
- `build/sbee-pack` — Linux virtual cartridge packager
- `build/test_vcart` — header parser tests
- `build/test_timing` — pure-C fixed-timestep tests
- `build/test_font` — rendered-pixel font regression test (when host tests are enabled)

The same source can be built with release optimization:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

CMake Debug/Release is **not yet** the final SugarBee developer/release
appliance-image distinction. Both variants use the same engine source.

## Build only the parser, packager, and tests

Useful when SDL2/Lua development packages are not installed:

```sh
cmake -S . -B build-tools -DSUGARBEE_BUILD_HOST=OFF
cmake --build build-tools --parallel
ctest --test-dir build-tools --output-on-failure
```

## Cartridge smoke tests (no SDL required)

`ctest` runs the examples selected in `CMakeLists.txt` through
[`tests/cartridge_smoke.lua`](../../tests/cartridge_smoke.lua) when a `lua5.4`
interpreter is installed (`sudo apt install lua5.4`). The script replaces the
`sugarbee` table with a stand-in that performs the same argument validation as
the real bindings — integer coordinates, RGBA buffer length, sprite regions
inside their texture, valid handles, host-reserved `start` — then calls
`init()`, 3000 fixed-step `update(dt)`/`draw()` pairs with seeded random
input, and `shutdown()`. Any Lua error that would have produced Guru
Meditation on the console fails the test with a traceback.

CMake checks the selected executable's `_VERSION` at configuration time. Only
`Lua 5.4` enables these optional tests. Missing, incompatible, or unusable
interpreters produce a configuration message and leave the native tests enabled.
The standalone interpreter check is separate from the engine's linked Lua library.

CMake caches the interpreter path. After installing Lua 5.4, rediscover it with:

```sh
cmake -S . -B build -U LUA54_INTERPRETER
```

Run one cartridge by hand, optionally with a frame count and seed:

```sh
lua5.4 tests/cartridge_smoke.lua examples/pacman-demo 3000 1
```

This is a crash test only. It draws nothing and cannot judge gameplay,
colours, or layout; the manual checks below remain necessary. `texture_load()`
reads the PNG header from the game directory for size checks, so run it on the
unpacked source folder, not a `.vcart`.

## Package and launch the graphics regression cartridge

The original gold-rectangle demonstration is checked in as
[`examples/graphics-demo/main.lua`](../../examples/graphics-demo/main.lua),
so its source does not depend on files under `/tmp` surviving between runs.
From the repository root:

```sh
./build/sbee-pack -i examples/graphics-demo -o /tmp/demo.vcart -t "SugarBee Demo"
./build/sugarbee /tmp/demo.vcart
```

Expected: a dark-blue background, gold rectangle, and white `SUGARBEE`
text beneath it. The packaged `.vcart` under `/tmp` is disposable; regenerate
it using the command above whenever needed.

The packager prints a randomly generated UUIDv4. **Save it in your game's
project metadata** and supply it with `--uuid <the-uuid>` for future builds,
so save files remain associated with the same game. A project manifest will
manage this automatically in a later milestone.

The host uses SDL2's window through the C graphics backend. Start/Enter opens
its pause menu; Exit Game returns to the host, where B/K closes SugarBee.
The window close control also exits. Fixed simulation timing, controller discovery,
hotplug, and a keyboard development fallback are implemented; analog input
is not yet implemented.

On normal exit after successful cartridge initialization, the host invokes an
optional Lua `shutdown()` before closing Lua and destroying input/graphics
services. It skips that callback while Guru Meditation is active. Errors in
`shutdown()` are reported to the console and native cleanup continues. The
callback has a provisional budget of 1,000,000 Lua instructions. This is
not a wall-clock timeout and cannot interrupt blocked native calls. See the [lifecycle reference](../api/timing.md)
and [manual shutdown regression procedure](../../tests/shutdown/README.md).

### Launcher

```sh
./build/sugarbee
./build/sugarbee --cartridges /path/to/vcarts
```

No arguments opens an idle screen with launch instructions. Directory mode
scans once and displays eligible entries; Up/Down selects, A/J launches, and
B/K closes the launcher. Exit Game returns to the list. Guru errors can be
dismissed with B/K. Direct `.vcart` launching remains available.

See [launcher behavior and limits](launcher.md) and the
[manual launcher checks](../../tests/launcher/README.md). Fixtures can be
regenerated after a reboot with:

```sh
bash tests/launcher/prepare-fixtures.sh
```

The script prints commands using its newly created temporary directory.

### Cartridge resource limits

The same bounded ZIP reader loads `main.lua` and requested assets. Its current
limits are a 64 MiB ZIP payload, 16 MiB per requested uncompressed member,
128 MiB total declared uncompressed size, and 1024 archive entries. It scans
the directory, validates paths, and verifies the requested member's actual
read length and CRC. It reads members into memory rather than extracting
files onto the host filesystem. See [asset limits](../api/assets.md) for details.

These are safeguards against excessive ZIP expansion, not comprehensive
ZIP-bomb protection or process isolation. There is no decompression-time
budget or aggregate per-game memory budget. Cartridges receive a restricted library set and instruction budgets for ordinary
runaway Lua loops. These do not form a hostile-cartridge sandbox or guarantee
recovery from native blocking, memory exhaustion, or repeatedly caught errors.
See the [supported libraries and execution limits](../api/timing.md).
Avoid running untrusted cartridges.

## Graphics regression check

The CTest suite includes a `graphics_smoke` test when host dependencies are
installed. It uses the SDL dummy video driver and checks the graphics backend
without opening a visible window. This is not a replacement for the manual
cartridge test above: verify that the dark blue background, gold rectangle,
and `SUGARBEE` text still render correctly. Verify no-argument idle startup and Guru error recovery as well. See [graphics API](../api/graphics.md).

The built-in font now covers printable ASCII, including real lowercase and
punctuation, using a pinned font8x8 source with an upstream public-domain
declaration. Attribution and the bit-order adaptation are recorded in
[font provenance](../../third_party/font8x8/README.md). The 8x8 character cells,
integer scaling, and newline spacing are unchanged; glyph shapes differ from
the earlier template font. `font_smoke` checks orientation, lowercase,
punctuation, scaling, newline placement, and unsupported-character fallback.
This is printable-ASCII rendering, not Unicode/UTF-8 decoding.

Guru error messages wrap within the viewport and show a continuation notice
when storage or display limits omit text. Complete supplied diagnostics go to
standard error. Follow the [manual Guru text test](../../tests/guru-text/README.md)
for word wrapping and vertical overflow checks. The shutdown and Guru test
cartridges are manual checks, not CTest entries.

## Known issues / limitations

- The graphics Lua bindings now call the language-neutral C graphics service,
  but Lua bindings still reside in `src/main.c`. Input polling and controller handling now live in
  `src/sb_input_sdl.c`, with a language-neutral `sb_input.h` interface.
- The bounded ZIP reader uses minizip 1.x against the complete `.vcart` with
  its 64-byte prefix; this has been tested on desktop Linux, but still needs
  verification on the Raspberry Pi and with additional minizip versions.
- Runtime graphics are currently fixed at 1280×720. Manifest-defined logical
  resolutions and scaling modes are planned, not implemented.
- The imported historical documents contain design ideas, not a verified
  description of the current software.

## Raspberry Pi

We plan to compile the C host natively on a 64-bit Raspberry Pi Linux system
first. This shares source code and Lua API with the Debian x86_64 host; it
is not a binary-compatible build. A cross-toolchain and minimal Linux image
will follow once the host's behavior is stable.

## Input demonstration (graphics + keyboard/gamepad)

After building with `SUGARBEE_BUILD_HOST=ON`:

```sh
./build/sbee-pack -i examples/moving-box -o /tmp/moving-box.vcart -t "Moving Box"
./build/sugarbee /tmp/moving-box.vcart
```

Move the gold box with arrow keys, W/A/S/D, or a connected controller's D-pad.
Press J or the controller's A button to print a message in the terminal.
Close the SDL window to quit. Test controller connection and disconnection
both before startup and while running. A real-controller regression test is
still manual; the headless suite covers keyboard input and optional virtual
controller hotplug. The input test tolerates controllers already plugged in,
so unplugging your development controller is not normally required. Avoid
pressing buttons or plugging/unplugging hardware during the automated test.
See [input API](../api/input.md) for mappings and current limitations. The example uses fixed 60 Hz `update(dt)` simulation. See the
[timing API](../api/timing.md) for timestep behavior, input-edge delivery,
and catch-up limits.

## Fixed-step regression check

After CTest, rerun the moving-box `.vcart` and confirm that holding a
direction moves the box smoothly. Briefly tap J or gamepad A: the terminal
should print one message per press, including when frames are rendered without
a simulation update. The original gold-rectangle and Guru screens must still
render. Verify the program closes normally with the window close button.

## Sprite demo (Milestone 3A)

After configuring and building the host, package and run the procedural bee
sprite demonstration:

```sh
./build/sbee-pack -i examples/sprite-demo -o /tmp/sprite-demo.vcart -t "Sprite Demo"
./build/sugarbee /tmp/sprite-demo.vcart
```

Expected: the dark-blue display shows `SUGARBEE SPRITES` and a small
black-and-gold sprite with transparent surroundings. Move it using the arrow
keys, WASD, or a controller D-pad. The `init()` callback creates a single
RGBA texture; `draw()` reuses it each frame.

This example generates pixel data in Lua. File-based PNG loading and sprite
transforms are also implemented; the asset and sprite-sheet examples below
exercise those paths. See [graphics API](../api/graphics.md).

`ctest --test-dir build --output-on-failure` also runs `texture_smoke` using
the SDL dummy driver, checking texture handle creation, invalid byte lengths,
stale handles and normal graphics-context cleanup. Verify the gold rectangle,
Moving Box and Guru Meditation screens after applying this changeset.

## Cartridge image assets (Milestone 3B)

Install `libpng-dev` as listed in [dependencies.md](dependencies.md) and
rerun CMake configuration after adding the package. The game in
`examples/asset-demo/` includes a real PNG inside `assets/`:

```sh
./build/sbee-pack -i examples/asset-demo -o /tmp/asset-demo.vcart -t "Asset Demo"
./build/sugarbee /tmp/asset-demo.vcart
```

Expected: a movable transparent bee, loaded by
`sugarbee.texture_load("assets/bee.png")`, rather than assembled as RGBA bytes
in Lua. The packager includes nested assets automatically. Test keyboard and
controller movement, the older procedural sprite, the gold rectangle, and the
idle startup and Guru recovery paths. The new CTest cases `asset_smoke` and
`png_smoke` validate the archive resource loader and PNG decoding.

The built-in image loader supports PNG only in this milestone; it is limited
to 2048 × 2048 pixels and applies ZIP entry and archive resource limits.
Cartridges receive the [documented restricted libraries](../api/timing.md#supported-lua-libraries),
but these asset limits and Lua instruction budgets do **not** constitute a
hostile-code sandbox. Run cartridges from sources you trust.
See [cartridge asset reference](../api/assets.md).

## Sprite-sheet demo (Milestone 3C)

The 3C example includes a four-frame PNG atlas (48 × 32 pixels per frame)
with 3× animated drawing, and separate samples of horizontal/vertical flips.

```sh
./build/sbee-pack -i examples/sprite-sheet-demo -o /tmp/sprite-sheet-demo.vcart -t "Sprite Sheet Demo"
./build/sugarbee /tmp/sprite-sheet-demo.vcart
```

Move the large bee with arrow keys, WASD, or the controller D-pad. It faces
left/right with horizontal movement. Watch wing motion, crisp enlarged pixels,
the four static frames along the bottom, and the horizontal/vertical mirrors.
The new `sprite_region()` API validates source bounds and sizes; the expanded
`texture_smoke` checks pixel-level crop, nearest-neighbor scale, and both flips
using the SDL dummy driver. `sprite_sheet_png_smoke` also validates the
192×32 atlas. Follow with the old gold-rectangle, Moving Box,
asset-demo, idle-startup, and Guru recovery regressions.

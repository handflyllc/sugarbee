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

- `build/sugarbee` — original SDL2/Lua host prototype
- `build/sbee-pack` — Linux virtual cartridge packager
- `build/test_vcart` — header parser tests
- `build/test_timing` — pure-C fixed-timestep tests

The same source can be built with release optimization:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
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

The existing host uses SDL2's desktop window through the C graphics backend.
Exit with the window close
control. Fixed simulation timing is implemented; game shutdown callbacks,
ZIP bomb limits, and process isolation are **not yet implemented**. Controller discovery,
hotplug, and a keyboard development fallback are implemented, but not analog input.
Avoid running untrusted cartridges.

## Graphics regression check

The CTest suite includes a `graphics_smoke` test when host dependencies are
installed. It uses the SDL dummy video driver and checks the graphics backend
without opening a visible window. This is not a replacement for the manual
cartridge test above: verify that the dark blue background, gold rectangle,
and `SUGARBEE` text still render after this refactor. Verify the no-cartridge
Guru screen as well. See [graphics API](../api/graphics.md).

## Known issues / limitations

- The bitmap font recovered from the older template implements ASCII 32–95;
  current fallback glyphs display lowercase as uppercase and map some other
  punctuation to `?` until a complete font is added.
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

The example generates pixel data in Lua and does **not** yet read images from
cartridge files. File-based cartridge assets, decoding and sprite transforms
are planned for separate milestones. See [graphics API](../api/graphics.md).

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
Guru Meditation no-cartridge path. The new CTest cases `asset_smoke` and
`png_smoke` validate the archive resource loader and PNG decoding.

The built-in image loader supports PNG only in this milestone; it is limited
to 2048 × 2048 pixels and applies ZIP entry and archive resource limits.
Cartridges should be treated as untrusted code: Lua currently opens its normal
standard libraries, so these limits do **not** constitute a Lua sandbox.
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
asset-demo and no-cartridge Guru regressions.

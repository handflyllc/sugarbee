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

## Package and launch a simple test cartridge

```sh
mkdir -p /tmp/sugarbee-demo
cat > /tmp/sugarbee-demo/main.lua <<'LUA'
function init() end
function update() end
function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.rect(50, 50, 100, 80, 240, 180, 50, true)
    sugarbee.print("SUGARBEE", 60, 150, 255, 255, 255, 2)
end
LUA

./build/sbee-pack -i /tmp/sugarbee-demo -o /tmp/demo.vcart -t "Demo"
./build/sugarbee /tmp/demo.vcart
```

The packager prints a randomly generated UUIDv4. **Save it in your game's
project metadata** and supply it with `--uuid <the-uuid>` for future builds,
so save files remain associated with the same game. A project manifest will
manage this automatically in a later milestone.

The existing host uses SDL2's desktop window. Exit with the window close
control. A controller manager, fixed simulation timestep, proper game shutdown
callback, ZIP bomb limits, and process isolation are **not yet implemented**.
Avoid running untrusted cartridges.

## Known issues / limitations

- The bitmap font recovered from the older template implements ASCII 32–95;
  current fallback glyphs display lowercase as uppercase and map some other
  punctuation to `?` until a complete font is added.
- The imported `main.c` still exposes SDL calls directly in Lua bindings;
  extraction into language-neutral SugarBee services is the next refactor.
- The ZIP reader currently calls `unzOpen` on the complete `.vcart` with its
  64-byte prefix. ZIP library behavior needs verification on target systems.
- Runtime graphics are currently fixed at 1280×720. Manifest-defined logical
  resolutions and scaling modes are planned, not implemented.
- The imported historical documents contain design ideas, not a verified
  description of the current software.

## Raspberry Pi

We plan to compile the C host natively on a 64-bit Raspberry Pi Linux system
first. This shares source code and Lua API with the Debian x86_64 host; it
is not a binary-compatible build. A cross-toolchain and minimal Linux image
will follow once the host's behavior is stable.

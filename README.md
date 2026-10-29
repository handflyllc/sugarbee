# SugarBee

SugarBee is an experimental 2D-first homebrew game-console runtime, initially
for Raspberry Pi 4 (64-bit Linux). The primary developer environment is Debian
12 x86_64. Games should target SugarBee services rather than SDL or Linux APIs.

**Status:** early prototype / API not stable. The graphics calls now use a
language-neutral C interface with an SDL2 backend. The input subsystem now
uses a separate C service for controllers and keyboard development controls;
the timing scheduler is now another language-independent C module, and
Lua `update(dt)` uses a fixed 60 Hz timestep. An initial texture-handle and
RGBA sprite path is available in both the C graphics core and Lua adapter.
Lua 5.4 is the first game language.

## Build

See [dependencies](docs/development/dependencies.md) and
[building](docs/development/building.md). The baseline uses CMake, SDL2, Lua
5.4, minizip, zlib, and the system `zip` command for cartridge packaging.

## Documentation

- [Build and testing](docs/development/building.md)
- [Dependencies](docs/development/dependencies.md)
- [Current `.vcart` format](docs/specifications/cartridge-format.md)
- [2D graphics API](docs/api/graphics.md)
- [Controller/keyboard input API](docs/api/input.md)
- [Simulation timing and callbacks](docs/api/timing.md)
- [Save/load API](docs/api/saves.md)
- [Current architecture](docs/architecture/overview.md)
- [Imported historical design](docs/architecture/project_sugarbee_technical_design_specification.md)

The historical design files are retained as references; not all their
proposals describe the present implementation.

## Repository policy

Engine changes, documentation, and relevant tests should be updated in the same
pull request. Release and developer builds share a source tree; appliance-image
profiles are future work.

**Licensing:** SugarBee's original code is available under the
[zlib license](LICENSE). Third-party material retains its own terms;
see the [embedded font provenance](third_party/font8x8/README.md).

**Milestone 3C preview:** Crop, scale, mirror and animate a sprite sheet from
one cartridge PNG using `sugarbee.sprite_region(...)`. Try
`examples/sprite-sheet-demo/` and see
[`docs/api/graphics.md`](docs/api/graphics.md). Existing `sprite(...)` calls
still work as before.

**Milestone 3B preview:** Load PNG assets from cartridges using
`sugarbee.texture_load("assets/bee.png")`. See `examples/asset-demo/` and
[`docs/api/assets.md`](docs/api/assets.md). Requires `libpng-dev` for the host.

The embedded font has separate documented provenance and an upstream public-domain
declaration; see [font attribution](third_party/font8x8/README.md).

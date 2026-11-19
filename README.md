# SugarBee

SugarBee is an experimental 2D-first homebrew game-console runtime, initially
for Raspberry Pi 4 (64-bit Linux). The primary developer environment is Debian
12 x86_64. Games should target SugarBee services rather than SDL or Linux APIs.

**Status:** experimental 0.1 baseline; APIs are provisional. The current
engine includes 2D graphics and PNG sprites, controller/keyboard input, fixed
60 Hz simulation, cartridge launching and lifecycle limits, explicit saves,
nine synth voices, native music assets/sequencing with Lua playback controls,
and eight WAV playbacks with whole-sound looping. Lua 5.4
is the first cartridge language. Source targets Debian x86_64 and Raspberry
Pi 4 ARM64; native binaries are platform-specific.

Native music was merged at `2bf169c`. The subsequent A0–C8 runtime at
`4d7ca11` has user-accepted desktop and Pi validation: 130/130 serial tests,
listening and cartridge/lifecycle checks. Acceptance is recorded in `7085043`;
see [runtime evidence](tests/audio-piano-runtime.md).
The published `v0.1.0-rc1` packages predate native music playback; see the
[release notes](RELEASE_NOTES.md) for that release's scope and compatibility.
No bootable console image is included. See the [roadmap](ROADMAP.md) for current
priorities and future work. SugarBeats develops independently against a pinned
engine revision; routine cross-project handoffs are no longer required.

## Build

See [dependencies](docs/development/dependencies.md) and
[building](docs/development/building.md). The baseline uses CMake, SDL2, Lua
5.4, minizip, zlib, and the system `zip` command for cartridge packaging.

## Documentation

- [The SugarBee Manifesto — purpose and principles](MANIFESTO.md)
- [Release-readiness checklist](docs/development/release-checklist.md)
- [Release notes](RELEASE_NOTES.md)
- [Build and testing](docs/development/building.md)
- [Dependencies](docs/development/dependencies.md)
- [Friction reconciliation and focused review](docs/development/friction-review-2026-11-16.md)
- [Current `.vcart` format](docs/specifications/cartridge-format.md)
- [Cartridge-local Lua modules](docs/api/modules.md)
- [Multi-file modules demo](examples/modules-demo/README.md)
- [2D graphics API](docs/api/graphics.md)
- [Canvas declaration and scaling](docs/api/canvas.md)
- [Canvas demo](examples/canvas-demo/README.md)
- [Text measurement demo](examples/text-metrics-demo/README.md)
- [Filled triangle demo](examples/triangle-demo/README.md)
- [Circle primitives demo](examples/circle-demo/README.md)
- [Line primitive demo](examples/line-demo/README.md)
- [Sprite tint demo](examples/tint-demo/README.md)
- [Camera demo](examples/camera-demo/README.md)
- [Clip-rectangle demo](examples/clip-demo/README.md)
- [Cartridge asset loading](docs/api/assets.md)
- [Cartridge data reader](docs/api/data.md)
- [Data reader demo](examples/data-demo/README.md)
- [Controller/keyboard input API](docs/api/input.md)
- [Simulation timing and callbacks](docs/api/timing.md)
- [Save/load API](docs/api/saves.md)
- [Interactive Save Lab](examples/save-lab/README.md)
- [Synth audio API](docs/api/audio.md)
- [Native WAV foundation and listening checks](tests/audio-wav.md)
- [Cartridge WAV API](docs/api/wav.md)
- [Experimental music asset and playback API](docs/api/music.md)
- [WAV Lab](examples/wav-lab/README.md)
- [Interactive Synth Lab](examples/synth-lab/README.md)
- [Music Lab](examples/music-lab/README.md)
- [Song-change example](examples/song-change/README.md)
- [Musical Synth Lab](examples/musical-synth-lab/README.md)
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

**Sprite-sheet example:** Crop, scale, mirror and animate a sprite sheet from
one cartridge PNG using `sugarbee.sprite_region(...)`. Try
`examples/sprite-sheet-demo/` and see
[`docs/api/graphics.md`](docs/api/graphics.md). Existing `sprite(...)` calls
still work as before.

**PNG asset example:** Load PNG assets from cartridges using
`sugarbee.texture_load("assets/bee.png")`. See `examples/asset-demo/` and
[`docs/api/assets.md`](docs/api/assets.md). Requires `libpng-dev` for the host.

The embedded font has separate documented provenance and an upstream public-domain
declaration; see [font attribution](third_party/font8x8/README.md).

# Engine architecture — current state (SugarBee 0.1)

SugarBee is an experimental, **2D-first** game console runtime. Game code
should target SugarBee services, not the operating system or SDL. The initial
game language is Lua 5.4; a language-independent core is the goal.

Current data flow for graphics:

```text
.vcart / main.lua
        |
Lua API bindings (src/main.c)
        |
Language-neutral C graphics interface (include/sb_graphics.h)
        |
SDL2 backend (src/sb_graphics_sdl.c)
        |
OS-provided display and input drivers
```

The SDL2 graphics backend owns its window, renderer, and RGBA sprite
textures. Games receive opaque numeric texture handles, never SDL texture
pointers. The C graphics core accepts raw in-memory RGBA bytes; the separate
3B cartridge reader and PNG decoder load images from `.vcart` payloads. The
3C `sprite_region()` call draws an in-bounds source rectangle to an optionally
scaled/flipped destination using nearest-neighbor filtering. Frame selection
and animation timing remain in Lua; SDL and Lua types do not leak into the
C graphics interface.

The SDL2 graphics backend owns its window and renderer. The language-neutral
input service (`include/sb_input.h`, implemented by `src/sb_input_sdl.c`) owns
controller handles and currently polls the SDL event queue. Lua's `btn` and
`btnp` functions in `src/main.c` read SugarBee logical button states, not SDL
objects. Fixed-step scheduling lives behind the pure-C `sb_timing.h` interface
(`src/sb_timing.c`); the host supplies elapsed time using SDL performance
counters and invokes Lua `update(dt)` with a fixed 1/60-second step. The host
still owns Lua, cartridge loading, and the main loop; these are not yet
independently modularized.

The same C source is intended to compile on Debian 12 x86_64 and a 64-bit
Raspberry Pi 4 Linux environment. This is **source portability**, not a shared
native binary. The initial game cartridge runs Lua code, which is independent
of the host CPU. Native cartridge games are not yet supported.

## Build and documentation policy

Source changes, their relevant tests, and current API reference changes belong
in the same pull request. All build profiles share one source tree. CMake's
Debug/Release modes currently control compiler builds; separate developer and
release console *images* have not been built yet.

On normal application exit, the host invokes an optional Lua `shutdown()`
callback after successful cartridge initialization, unless Guru Meditation
is active. It runs before Lua, input, and graphics are destroyed. Callback
errors are reported to the console, and native cleanup continues.
Cartridge switching and returning to a menu are not implemented.

## Agreed design goals that are not yet features

- Manifest-defined logical resolutions (default 1280 x 720), aspect-preserving
  scaling, and pixel-art integer scaling.
- A full input event router, analog controls, and lossless input-event queues.
  Basic button-press delivery across zero/multiple simulation updates works.
- Save APIs tied to persistent cartridge UUIDs.
- A minimal Linux appliance image and a development image from shared build
  configuration.
- Language adapters beyond Lua and an extensible plugin system (deferred).

The imported design documents in this folder are **historical drafts**.
They may differ from the currently implemented behavior; refer to source and
current reference documentation for the present API.

## Cartridge asset path (Milestone 3B)

`Lua adapter (texture_load)` → `sb_assets` (bounded ZIP reads) → `sb_png`
(libpng memory decode) → `sb_graphics_texture_rgba()` → SDL2 texture handle.
Neither `sb_assets` nor `sb_png` depends on Lua or SDL2; future language
adapters can reuse them. The packager and header format remain unchanged.
The PNG backend is a replaceable implementation detail, not a public ABI.

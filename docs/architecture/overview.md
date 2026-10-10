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

The SDL2 graphics backend owns its window and renderer. The language-neutral
input service (`include/sb_input.h`, implemented by `src/sb_input_sdl.c`) owns
controller handles and currently polls the SDL event queue. Lua's `btn` and
`btnp` functions in `src/main.c` read SugarBee logical button states, not SDL
objects. The host still owns Lua, cartridge loading, and the main loop; these
are not yet independently modularized.

The same C source is intended to compile on Debian 12 x86_64 and a 64-bit
Raspberry Pi 4 Linux environment. This is **source portability**, not a shared
native binary. The initial game cartridge runs Lua code, which is independent
of the host CPU. Native cartridge games are not yet supported.

## Build and documentation policy

Source changes, their relevant tests, and current API reference changes belong
in the same pull request. All build profiles share one source tree. CMake's
Debug/Release modes currently control compiler builds; separate developer and
release console *images* have not been built yet.

## Agreed design goals that are not yet features

- Manifest-defined logical resolutions (default 1280 x 720), aspect-preserving
  scaling, and pixel-art integer scaling.
- A full input event router, analog controls, and robust fixed-step input delivery.
- Fixed simulation timing, game shutdown callbacks, and save APIs tied to
  persistent cartridge UUIDs.
- A minimal Linux appliance image and a development image from shared build
  configuration.
- Language adapters beyond Lua and an extensible plugin system (deferred).

The imported design documents in this folder are **historical drafts**.
They may differ from the currently implemented behavior; refer to source and
current reference documentation for the present API.

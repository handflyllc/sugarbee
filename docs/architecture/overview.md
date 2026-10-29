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

The graphics backend also owns a retained game-frame render-target texture.
During gameplay, Lua drawing targets that texture; the host then copies it
to the window. During pause, the host reuses the retained image without
calling Lua update or draw. Host overlays are drawn onto the window, leaving
the saved game image unchanged. The selected SDL renderer must support
render-target textures.

The same C source is intended to compile on Debian 12 x86_64 and a 64-bit
Raspberry Pi 4 Linux environment. This is **source portability**, not a shared
native binary. The initial game cartridge runs Lua code, which is independent
of the host CPU. Native cartridge games are not yet supported.

## Build and documentation policy

Source changes, their relevant tests, and current API reference changes belong
in the same pull request. All build profiles share one source tree. CMake's
Debug/Release modes currently control compiler builds; separate developer and
release console *images* have not been built yet.

When an initialized cartridge session ends, the host invokes its optional
Lua `shutdown()` callback unless Guru Meditation is active. The callback
runs before Lua and cartridge textures are released, while input and graphics
services remain available. Callback errors are reported to the console,
and cleanup continues.

The host pause menu offers Resume and Exit Game. Exit Game requests a
transition that the main loop handles after finishing its current input
step. `end_cartridge()` invokes the eligible shutdown callback, closes Lua,
releases cartridge textures, and clears session and pause state.

The host then displays an idle screen without calling cartridge update or
draw callbacks. Its window, renderer, retained-frame texture, and controllers
remain alive. B or keyboard K closes the idle host in desktop mode.
With the explicit leading `--console` flag, idle B/K does nothing and Start/Enter
opens a test-only shutdown confirmation. Cancel is selected initially; the
confirmed test reports a request and keeps the host running. No OS power-off
is implemented. The panel blocks underlying launcher actions, and in-game
pause/exit behavior remains unchanged. See [launcher controls](../development/launcher.md).

A or keyboard J requests relaunch of the startup cartridge. The host retains
the argv-owned startup path separately from the active session path.
`start_cartridge()` creates a fresh Lua state and initializes the cartridge.
Launch input is consumed and held buttons are suppressed until released.

Starting without arguments opens the idle host. `--cartridges DIRECTORY`
scans top-level regular files with a case-sensitive `.vcart` suffix, skipping
symbolic links and subdirectories. `inspect_cartridge()` uses a local header
and the bounded asset reader to verify that `main.lua` is readable without
compiling or executing it. Launch still validates and loads the selected file;
a discovery result is not a guarantee of successful execution.

The engine stores up to 64 eligible title/path pairs in a fixed array. Paths
occupy 4096-byte buffers including termination. The first 64 eligible entries
are retained, then sorted by case-sensitive title and filename. Additional
eligible files are reported but not stored, so the retained subset above
capacity still depends on discovery order. The active session
borrows its path from argv or this stable array. The list is not rescanned while
a cartridge is running. The host renders eight rows with a selection marker,
filenames to distinguish duplicate titles, and bounded printable-ASCII labels.
A/J requests launch after the current input step; Exit Game preserves the list
and selection. Direct-path launch and relaunch remain supported.

Failed launches and runtime Lua errors display Guru Meditation. B/K consumes
the recovery input, ends the failed session while Guru remains active (skipping
Lua shutdown), clears the Guru flag, and resets timing before returning to the
host. Main-thread Lua instruction budgets now catch ordinary runaway loops in
main.lua execution, init, update, draw, and shutdown. Shutdown limit failures
report to the console and continue cleanup without Guru. These limits do not
cover blocked native calls, state-closure finalizers, or deliberate bypasses;
see [execution limits and lifecycle](../api/timing.md).

X/U requests a rescan while the launcher is idle. The host performs it after
the input step, resets selection to the first sorted entry, and resets timing.
The screen reports capacity, omitted/rejected counts, and directory errors.
Only the startup scan can automatically launch a sole eligible cartridge, and
only with no omitted entries or scan errors. Exit, rescan, and Guru recovery
leave the launcher available without an automatic relaunch loop.
See [launcher usage](../development/launcher.md).



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

# SugarBee graphics API (experimental v0.1)

This page describes the **implemented** graphics interface after the first
language-independent extraction. It is not yet a stable public API or ABI.

## How drawing is organized

A cartridge calls Lua's `sugarbee` graphics functions. The Lua bindings in
`src/main.c` convert Lua arguments into calls to the language-neutral C
interface declared in `include/sb_graphics.h`. `src/sb_graphics_sdl.c` owns
SDL2's window and renderer; neither SDL nor Lua is exposed by the C graphics
header. Future language adapters may call the same C services.

The host initializes SDL's video subsystem, creates a graphics context, and
presents the frame. Game code does not own or access an SDL renderer.

## Current Lua functions

All coordinates and dimensions are in **logical pixels**. Color arguments
are integer red, green, and blue components; use values from 0 to 255. The
current Lua bindings convert values to 8-bit channels. All drawing is opaque.

| Lua call | Behavior |
| --- | --- |
| `sugarbee.cls(r, g, b)` | Clear the frame to a color. Missing channels default to 0. |
| `sugarbee.rect(x, y, w, h, r, g, b, filled)` | Draw a rectangle. Color channels default to 255. `filled` is false when omitted or `false`. |
| `sugarbee.print(text, x, y, r, g, b, scale)` | Draw text with the built-in 8x8 font. Color defaults to white; `scale` defaults to 1. |

`x`, `y`, `w`, `h`, and `scale` are integral values in the current binding.
Lua treats every value except `false` and `nil` as true, including `0`;
therefore pass an actual boolean for `filled`.

### Example (current API)

```lua
function init() end
function update() end
function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.rect(50, 50, 100, 80, 240, 180, 50, true)
    sugarbee.print("SUGARBEE", 60, 150, 255, 255, 255, 2)
end
```

The bitmap font is incomplete: the imported historical font covers only part
of ASCII, with temporary fallback glyphs for the remaining printable characters.
A newline advances 10 scaled logical pixels and resets the text cursor to its
initial x position. Unsupported characters are rendered as `?`.

## Logical resolution and scaling

The current runtime uses a **fixed 1280 x 720 logical size**, with SDL's logical
rendering scale. Games cannot yet request an alternate viewport or integer/
nearest-neighbor scaling policy. The agreed **future** design is:

- Default to 1280 x 720 when the cartridge does not specify a viewport.
- Permit other logical sizes (such as 320 x 200 or 640 x 480).
- Preserve the aspect ratio and use black borders rather than stretching.
- Offer suitable scaling modes for pixel art.

These features are planned, not implemented by this changeset.

## Internal C interface

See [`include/sb_graphics.h`](../../include/sb_graphics.h) for the authoritative
current declarations. Its operations are:

- `sb_graphics_create()` / `sb_graphics_destroy()` — manage a context owning an
  SDL2 window and renderer. SDL video must be initialized first.
- `sb_graphics_width()` / `sb_graphics_height()` — query logical dimensions.
- `sb_graphics_clear()`, `sb_graphics_rect()`, `sb_graphics_text()` — draw in
  logical coordinates, without depending on Lua.
- `sb_graphics_present()` — display the completed frame.

Use these functions on the host's video thread. The SDL2 backend first requests
an accelerated, synchronized renderer and falls back to software rendering when
unavailable (useful for headless development tests).

This is an **internal interface**, not a stable ABI for native game modules.
Memory ownership, version negotiation, and native-game isolation remain future
work. No sprites, transforms, or 3D support are implemented here.

## Testing

`ctest` now includes `graphics_smoke` when `SUGARBEE_BUILD_HOST=ON`. It uses
SDL's dummy video driver to exercise context creation, logical dimensions,
rectangle and text calls, frame presentation, and cleanup. This is a headless
smoke test, **not** a pixel-perfect renderer verification.

The earlier gold-rectangle test cartridge remains the manual end-to-end
regression test for Lua bindings, cartridge loading, and on-screen output.

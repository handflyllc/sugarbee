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
current Lua bindings convert values to 8-bit channels. Rectangles and text are opaque; RGBA sprite textures support transparency.

| Lua call | Behavior |
| --- | --- |
| `sugarbee.cls(r, g, b)` | Clear the frame to a color. Missing channels default to 0. |
| `sugarbee.rect(x, y, w, h, r, g, b, filled)` | Draw a rectangle. Color channels default to 255. `filled` is false when omitted or `false`. |
| `sugarbee.print(text, x, y, r, g, b, scale)` | Draw text with the built-in 8x8 font. Color defaults to white; `scale` defaults to 1. |
| `sugarbee.texture_rgba(width, height, bytes)` | Upload packed RGBA8 bytes; returns a texture handle. |
| `sugarbee.sprite(handle, x, y)` | Draw a texture at native pixel size in logical coordinates. |
| `sugarbee.texture_free(handle)` | Release a texture handle; returns `true` when successful. |

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

## Drawing order and viewport boundaries

SugarBee uses **submission order** for 2D drawing: later draw calls appear
above earlier ones wherever their pixels overlap. To place interface text over
a sprite, draw the sprite first and the text afterward. There is no explicit
layer or depth API yet.

Sprite and shape positions are expressed in logical coordinates and **may be
outside the viewport**. SugarBee does not clamp object positions or stop game
objects at the screen edge. The SDL renderer clips drawing to the visible
viewport; offscreen pixels are not displayed. Games implement their own
movement limits, cameras, and world boundaries when needed.

For an example of primitive drawing and text order, see
[`examples/graphics-demo/main.lua`](../../examples/graphics-demo/main.lua).

The bitmap font is incomplete: the imported historical font covers only part
of ASCII, with temporary fallback glyphs for the remaining printable characters.
A newline advances 10 scaled logical pixels and resets the text cursor to its
initial x position. Unsupported characters are rendered as `?`.

## Prototype sprites and texture ownership (Milestone 3A)

Textures are **opaque integer handles** managed by the SugarBee graphics
context. A handle is valid only for that running game/graphics context.
The handle `0` is always invalid. Destroying a handle invalidates it; using
an invalid handle with `sugarbee.sprite` produces a Lua error.

The preliminary Lua API accepts a Lua **binary string** containing exactly
`width * height * 4` bytes in **R, G, B, A order** (one byte per channel, row
by row, starting at the top left). Alpha controls transparency, and SugarBee
copies the pixels during upload, so the Lua string may be discarded afterward.
Create textures in `init()`, not on every `draw()` call. Release temporary
textures with `texture_free()`; any remaining textures are freed when the
host's graphics context shuts down.

The C graphics service is deliberately independent of files and image formats:
`sb_graphics_texture_rgba()` accepts an in-memory RGBA buffer. This gives
cartridge asset loading a single clear destination when it arrives in **3B**.
The SDL backend currently draws at native texture size only; scaling,
rotation, source rectangles and animation frames belong to a later phase.

The sample `examples/sprite-demo/main.lua` builds a small pixel-art sprite
in Lua, moves it using `update(dt)`, and displays it using `sprite()`. Pack and
run it using the commands in the build guide.

**Limitations:** This phase does not load PNG/BMP files, decode images from
`.vcart` assets, specify a stable texture-handle ABI, enforce a memory budget,
or add sprite scaling/rotation. These are future tasks, not current claims.

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
- `sb_graphics_texture_rgba()` — upload tightly packed RGBA8 pixels; copies the
  caller's buffer, returns an opaque context-owned handle (or `0` on failure).
- `sb_graphics_sprite()` / `sb_graphics_texture_destroy()` — draw and release
  handles; both return false when given an unknown/stale handle.
- `sb_graphics_present()` — display the completed frame.

Use these functions on the host's video thread. The SDL2 backend first requests
an accelerated, synchronized renderer and falls back to software rendering when
unavailable (useful for headless development tests).

This is an **internal interface**, not a stable ABI for native game modules.
Memory ownership is defined for uploaded texture buffers (copied) and texture
handles (context-owned), but broader ABI versioning and native-game isolation
remain future work. No transforms or 3D support are implemented here.

## Testing

`ctest` includes `graphics_smoke` and (as of 3A) `texture_smoke` when
`SUGARBEE_BUILD_HOST=ON`. It uses
SDL's dummy video driver to exercise context creation, logical dimensions,
rectangle and text calls, sprite upload/drawing, handle invalidation and
cleanup. The texture test reads back RGBA pixels under SDL dummy rendering to
check color and transparency. This remains a headless smoke test, **not** full visual verification.

The earlier gold-rectangle test cartridge remains the manual end-to-end
regression test for Lua bindings, cartridge loading, and on-screen output.

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

## Logical canvas

Cartridges may declare a fixed canvas in root `console.json`; existing cartridges
retain 1280×720. See [declaration, supported sizes and scaling](canvas.md).
Host UI uses a separate canvas. `sugarbee.width()` and `sugarbee.height()` always
report the game's dimensions, including outside `draw()`.

## Current Lua functions

All coordinates and dimensions are in **logical pixels**. Color arguments
are integer red, green, and blue components; use values from 0 to 255. The
current Lua bindings convert values to 8-bit channels. Rectangles and text accept optional per-call alpha, defaulting to opaque. RGBA sprite textures blend partial alpha with
the pixels already drawn: alpha 0 is transparent, 255 is opaque, and intermediate
values are translucent. This per-pixel alpha combines with optional sprite tint opacity as described
below. Text, rectangles, lines, circles and triangles are unaffected by sprite tint.

| Lua call | Behavior |
| --- | --- |
| `sugarbee.sprite_rotated(texture, x, y, angle [, pivot_x, pivot_y])` | Rotate a whole texture at native size; see rotation contract below. |
| `sugarbee.sprite_region_rotated(texture, x, y, sx, sy, sw, sh, dw, dh, angle [, pivot_x, pivot_y])` | Rotate a scaled source region; destination dimensions are required. |
| `sugarbee.width()` / `sugarbee.height()` | Return the fixed game canvas dimensions; no arguments. |
| `sugarbee.sprite_tint(r, g, b [, a])` | Set sprite RGB tint and optional opacity. |
| `sugarbee.sprite_tint_reset()` | Restore white tint and full opacity. |
| `sugarbee.camera(x, y)` | Replace the camera position used by drawing. |
| `sugarbee.camera_reset()` | Restore screen coordinates (camera at zero). |
| `sugarbee.clip(x, y, w, h)` | Replace the drawing clip; dimensions must be nonnegative. |
| `sugarbee.clip_reset()` | Disable the drawing clip. |
| `sugarbee.cls(r, g, b)` | Clear the frame to a color. Missing channels default to 0. |
| `sugarbee.triangle_fill(x1, y1, x2, y2, x3, y3 [, r, g, b])` | Draw an opaque filled triangle with top-left edge ownership. |
| `sugarbee.circle(cx, cy, radius [, r, g, b])` | Draw the one-pixel boundary of a disk. |
| `sugarbee.circle_fill(cx, cy, radius [, r, g, b])` | Draw a solid disk. |
| `sugarbee.line(x1, y1, x2, y2 [, r, g, b])` | Draw an opaque one-pixel line, including both endpoints. |
| `sugarbee.rect(x, y, w, h, r, g, b, filled [, alpha])` | Draw a rectangle. Color channels default to 255. `filled` is false when omitted or `false`. |
| `sugarbee.measure_text(text [, scale])` | Return character-cell width and height without drawing. |
| `sugarbee.print(text, x, y, r, g, b, scale [, alpha])` | Draw text with the built-in 8x8 font. Color defaults to white. `scale` is an optional integer scale factor, defaulting to 1; values of 0 or less draw nothing. |
| `sugarbee.texture_rgba(width, height, bytes)` | Upload packed RGBA8 bytes; returns a texture handle. |
| `sugarbee.texture_load(path)` | Decode a PNG from the active `.vcart` and return a texture handle. |
| `sugarbee.sprite(handle, x, y)` | Draw a texture at native pixel size in logical coordinates. |
| `sugarbee.sprite_region(handle, x, y, sx, sy, sw, sh [, dw, dh, flip_x, flip_y])` | Crop from a sprite sheet, optionally resize and mirror. |
| `sugarbee.texture_free(handle)` | Release a texture handle; returns `true` when successful. |

`x`, `y`, `w`, `h`, and `scale` must be integer-valued and fit the native signed
32-bit range on supported desktop/Pi builds (-2147483648 through 2147483647).
Out-of-range values raise Lua argument errors before drawing; they do not wrap
or clamp. Integer-valued floats such as `12.0` remain accepted. Legacy rectangle/text color conversion is unchanged; line, circle, triangle and sprite-tint
channels are range-checked as documented below. Text positions and scaled glyphs are clipped safely
at the viewport, including extreme coordinates and scales; nonpositive text
scale still draws nothing.
Keep fractional positions in simulation state and convert at the drawing boundary,
for example `sugarbee.sprite(bee, math.floor(x), math.floor(y))`. Do not round
the simulation position every update; doing so loses subpixel motion.

Lua treats every value except `false` and `nil` as true, including `0`;
therefore pass an actual boolean for `filled`.

### Text measurement

```lua
local width, height = sugarbee.measure_text(text, scale)
```

Returns exactly two integers in logical pixels, independent of canvas size,
camera, clip, sprite tint and drawing target. It neither draws nor changes
rendering state; it can be called during top-level code and lifecycle callbacks.
These are **character-cell bounds**, not the tight bounds of illuminated pixels.

- Scale defaults to 1 when omitted or `nil`; integer-valued scales must fit the
  signed 32-bit range, using the same conversion as `print()`.
- Each non-newline byte occupies an 8×8 cell; newline advances 10 pixels
  vertically and resets the horizontal position. Both dimensions scale.
- Width is the longest line, including leading/trailing spaces. Height is
  `8 + 10 * (line_count - 1)` before scaling, including blank and trailing lines.
  The final line has an 8-pixel cell height, without an extra trailing gap.
- Empty text returns `0, 0`. Nonpositive scale also returns `0, 0`, matching
  `print()` drawing nothing. A newline alone returns `0, 18` at scale 1.
- Text uses the existing byte-oriented font rules: printable ASCII is supported;
  other bytes occupy one fallback-glyph cell each. Tabs and carriage returns are
  fallback cells, not tab stops or line breaks. UTF-8 is not decoded.
- An embedded NUL ends the measured text, matching the existing native renderer.
  Numeric text arguments are converted to strings as with `print()`; other
  non-string types are errors. The function accepts only one or two arguments.
- A measurement accepts at most **256 KiB of input**, counting the full Lua
  string, including bytes after NUL. Longer input is an error even at scale zero.
  Positive-scale dimensions exceeding `2147483647` are errors rather than wrapped
  or clamped results. These limits affect measurement only; `print()` is unchanged.

| Text (Lua notation) | Scale | Width | Height |
| --- | --- | --- | --- |
| `"Paused"` | 1 | 48 | 8 |
| `"Two\nlines"` | 1 | 40 | 18 |
| `" A  "` | 2 | 64 | 16 |
| `"End\n"` | 1 | 24 | 18 |
| `"\n"` | 1 | 0 | 18 |
| `""` | 1 | 0 | 0 |

For centered text, measure it with the same scale used for drawing:

```lua
local text, scale = "Paused", 2
local w, h = sugarbee.measure_text(text, scale)
sugarbee.print(text,
    math.floor((sugarbee.width() - w) / 2),
    math.floor((sugarbee.height() - h) / 2),
    255, 255, 255, scale)
```

See the [text measurement demo](../../examples/text-metrics-demo/README.md).
Automatic wrapping, alignment modes, new fonts and text input are separate work.

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

### Rectangle and text opacity

The final `alpha` argument is an integer in **0..255**; omitted or `nil` means
255. Zero draws nothing; intermediate values use ordinary source-over blending
with pixels already drawn. Invalid alpha raises a Lua error before drawing,
even when the geometry would be invisible. Existing RGB conversion and other
argument behavior remain unchanged. Calls return no values.

Opacity is per call, independent of sprite tint. Camera and clip still apply;
text measurement, glyphs, scaling and line spacing are unchanged. Translucent
rectangle borders cover corners once, including positive one-pixel dimensions.
Nonpositive dimensions draw nothing in the translucent path; opaque calls retain
the existing SDL rectangle behavior. Drawing order matters for overlapping calls.
Renderer blend mode is restored after the call; subsequent opaque drawing and
host menus do not inherit opacity. No alpha was added to other primitives.

```lua
sugarbee.rect(20, 20, 280, 80, 0, 0, 0, true, 160)
sugarbee.print("Welcome aboard.", 30, 30) -- opaque text
```

Native code can use `sb_graphics_rect_alpha()` and `sb_graphics_text_alpha()`;
existing C signatures remain available with opaque rendering. See the
[opacity demo and validation](../../examples/opacity-demo/README.md).

## Drawing order and viewport boundaries

SugarBee uses **submission order** for 2D drawing: later draw calls appear
above earlier ones wherever their pixels overlap. To place interface text over
a sprite, draw the sprite first and the text afterward. There is no explicit
layer or depth API yet.

Sprite and shape positions are expressed in logical coordinates and **may be
outside the viewport**. SugarBee does not clamp object positions or stop game
objects at the screen edge. The SDL renderer clips drawing to the visible
viewport; offscreen pixels are not displayed. Games implement their own
movement limits, camera following, and world boundaries when needed.

For an example of primitive drawing and text order, see
[`examples/graphics-demo/main.lua`](../../examples/graphics-demo/main.lua).

The bitmap font covers printable ASCII, including lowercase letters. Its source,
public-domain declaration, and bit-order adaptation are documented in
[`third_party/font8x8/README.md`](../../third_party/font8x8/README.md).
A newline advances 10 scaled logical pixels and resets the text cursor to its
initial x position. Unsupported characters are rendered as `?`.

## Clip rectangles

`clip(x, y, w, h)` takes exactly four integer-valued arguments within the native
signed integer range. `clip_reset()` takes none. Both return no values. Invalid
arguments raise Lua errors before changing the clip; a renderer failure also
raises an error. Negative width/height is invalid. Zero width or height creates
an empty clip that hides drawing until reset or replacement.

There is one active clip, in screen logical pixels. Setting another **replaces**
it; there is no nested stack. The visible area is the intersection with the
viewport, with half-open edges: `x <= pixel_x < x+w`, `y <= pixel_y < y+h`.
Negative/offscreen positions are allowed; a disjoint rectangle hides drawing.
Sprites (including regions/scaling/flips), lines, circles, triangles, filled/outline rectangles and
text obey the clip. Clipping itself does not translate positions or resize the window. A camera
translates drawing independently; the clip always remains in screen coordinates.

`cls()` always clears the whole frame and **preserves** the selected clip.
At the start of each game frame, before `draw()`, clipping is disabled. Set
clips inside `draw()`; a setting made only during `init()` or `update()` does
not carry into that drawing callback. Reset explicitly when drawing UI outside
a playfield. Host menus, Guru screens and subsequent cartridge sessions do not
inherit the cartridge clip.

```lua
function draw()
    sugarbee.cls(0, 0, 0)
    sugarbee.clip(160, 0, 960, 720) -- centered 4:3 playfield in 1280x720
    sugarbee.rect(100, 100, 200, 100, 255, 200, 60, true)
    sugarbee.clip_reset()
    sugarbee.print("HUD", 10, 10)
end
```

See the [clip demo and acceptance checklist](../../examples/clip-demo/README.md).
This API requires the clip-enabled engine; earlier engines have no `clip` or
`clip_reset` functions. Existing cartridges that never call them retain full-frame
drawing. Configurable resolution and nested clipping remain
separate future capabilities.

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

The C graphics service is independent of files and image formats:
`sb_graphics_texture_rgba()` accepts an in-memory RGBA buffer. Milestone 3B
adds a cartridge reader and PNG decoder *outside* the graphics backend,
then passes decoded RGBA pixels to this function.
The original `sprite(handle, x, y)` still draws the complete texture at
native size. Milestone 3C adds `sprite_region()` for source rectangles,
destination scaling, and optional flips. Arbitrary rotation and a built-in
animation system are not implemented.

The sample `examples/sprite-demo/main.lua` builds a small pixel-art sprite
in Lua, moves it using `update(dt)`, and displays it using `sprite()`. Pack and
run it using the commands in the build guide.

**Limitations:** PNG files can now be loaded from `.vcart` assets, but BMP,
JPEG, animated-file decoding, hot reload, sprite rotation, stable native-game ABI,
and a configurable texture-memory budget remain out of scope. An individual
PNG is limited to 2048 × 2048 pixels.

### PNG cartridge example (Milestone 3B)

Put `assets/bee.png` alongside `main.lua` in the game directory before
packaging. Load it once in `init()`:

```lua
local bee
function init()
    bee = sugarbee.texture_load("assets/bee.png")
end
function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.sprite(bee, 100, 100)
end
```

Paths are *case-sensitive*, ZIP-relative paths with forward slashes. Absolute
paths and `.`/`..` path segments are rejected. Missing, invalid, encrypted or
oversized assets cause a Lua error; a Lua runtime error activates Guru Meditation.
Image decompression takes place in a standalone libpng decoder, not SDL or Lua.
The host reads resources only from the active cartridge, never from arbitrary
host filesystem paths. See [cartridge assets](assets.md) for limits.

## Source rectangles, scaling, and sprite-sheet animation (Milestone 3C)

`texture_load()` loads the **entire PNG once** into a texture handle. To draw
just one cell from a horizontal sprite sheet, select the cell using source
coordinates relative to the *texture* (not the screen):

```lua
local sheet = sugarbee.texture_load("assets/bee-sheet.png") -- 4 x 48x32
local frame = 2  -- zero-based frame index
sugarbee.sprite_region(sheet, 100, 100, frame * 48, 0, 48, 32,
                       144, 96, false, false)
```

Arguments after the handle are `x, y` (destination top left), `sx, sy`
(source top left), `sw, sh` (source size), optional `dw, dh` (destination
size), and optional `flip_x, flip_y` booleans. Source coordinates and sizes
are integer **logical image pixels**; the source must fit entirely inside the
texture and have positive dimensions. The destination must also have positive
width and height. Omit both destination sizes to draw at source size; omitted
flip flags default to `false`. The last two flags flip the **drawn image**
inside its destination rectangle, not its position. Invalid regions or stale
handles raise Lua errors; partially off-screen destinations are clipped.

The backend uses **nearest-neighbor sampling** for per-sprite resizing, so
pixel-art edges remain crisp even with non-integer scaling. This does *not*
yet define the final logical-to-physical display scaling policy.

An animation is simply a changing source rectangle. Update a frame index in
`update(dt)` and draw it with `sprite_region()`; no animation scheduler or
texture allocation per frame is needed. See
[`examples/sprite-sheet-demo/`](../../examples/sprite-sheet-demo/) for a
four-frame, gamepad-movable bee, scaling, and both flip axes. The example
uses 8 animation frames per second atop SugarBee's fixed 60 Hz simulation.

## Logical resolution and scaling

See the [canvas contract](canvas.md) for root `console.json`, supported sizes,
integer enlargement and proportional downscale fallback. Host UI is separate
from the game canvas. Window size and physical display mode are unchanged.

## Internal C interface

See [`include/sb_graphics.h`](../../include/sb_graphics.h) for the authoritative
current declarations. Its operations are:

- `sb_graphics_create()` / `sb_graphics_destroy()` — manage a context owning an
  SDL2 window and renderer. SDL video must be initialized first.
- `sb_graphics_width()` / `sb_graphics_height()` — query the active drawing
  canvas (game or host).
- `sb_graphics_canvas_width()` / `sb_graphics_canvas_height()` — query the fixed
  game canvas independently of the active drawing target.
- `sb_graphics_set_canvas()` — replace and initialize the retained game texture.
- `sb_graphics_begin_game_frame()` / `sb_graphics_begin_host_frame()` — select
  the appropriate target, dimensions and default drawing state. Game begin clears.
- `sb_graphics_show_game_frame()` — scale the retained game image onto the output,
  then restore host coordinates for overlays.
- `sb_graphics_measure_text()` — bounded cell metrics with no graphics context,
  allocation or renderer dependency in the implementation; returns false for
  invalid pointers, excessive input or unrepresentable dimensions.
- `sb_graphics_clear()`, `sb_graphics_rect()`, `sb_graphics_text()` — draw in
  logical coordinates, without depending on Lua.
- `sb_graphics_texture_rgba()` — upload tightly packed RGBA8 pixels; copies the
  caller's buffer, returns an opaque context-owned handle (or `0` on failure).
- `sb_graphics_sprite()` / `sb_graphics_sprite_region()` — draw full textures
  or source rectangles into optionally scaled/flipped destinations. Both
  return false for invalid/stale handles; region draws also validate bounds.
- `sb_graphics_texture_destroy()` — release texture handles, rejecting stale
  or unknown handles.
- `sb_graphics_present()` — display the completed frame.

Use these functions on the host's video thread. The SDL2 backend first requests
an accelerated, synchronized renderer and falls back to software rendering when
unavailable (useful for headless development tests).

This is an **internal interface**, not a stable ABI for native game modules.
Memory ownership is defined for uploaded texture buffers (copied) and texture
handles (context-owned), but broader ABI versioning and native-game isolation
remain future work. The implemented transforms are crop, resize, and flip; no
arbitrary rotation or 3D support is implemented.

## Testing

`ctest` includes `graphics_smoke` and (as of 3A) `texture_smoke` when
`SUGARBEE_BUILD_HOST=ON`. It uses
SDL's dummy video driver to exercise context creation, logical dimensions,
rectangle and text calls, sprite upload/drawing, handle invalidation and
cleanup. The texture test reads back RGBA pixels under SDL dummy rendering to
check color and transparency. This remains a headless smoke test, **not** full visual verification.

The earlier gold-rectangle test cartridge remains the manual end-to-end
regression test for Lua bindings, cartridge loading, and on-screen output.

## Camera position

`sugarbee.camera(x, y)` takes exactly two integer-valued numbers in the native
signed integer range (signed 32-bit on supported targets). Negative positions
are allowed. `sugarbee.camera_reset()` takes no arguments. Both return no values.
Invalid arguments raise a Lua error without changing the previous camera.

Drawing uses **screen position = world position - camera position** for `rect`,
`line`, `circle`, `circle_fill`, `triangle_fill`, `print`, `sprite` and `sprite_region`. Dimensions, source crops, flips, text
spacing and scaling are unchanged. Camera calls replace the position rather
than accumulating it. Reset selects `(0, 0)`.

`clip` remains in screen coordinates regardless of call order. `cls` clears the
whole frame while preserving camera and clip. Both states reset before each
`draw()` and are isolated from host screens and subsequent cartridge sessions.
Set the camera inside `draw()`; setting it only in `init()` or `update()` does
not carry through to drawing. Existing cartridges default to screen coordinates.

```lua
sugarbee.clip(panel_x, panel_y, panel_w, panel_h)
-- Put the desired world origin at the panel's upper-left corner.
sugarbee.camera(world_left - panel_x, world_top - panel_y)
draw_world()
sugarbee.camera_reset()
sugarbee.clip_reset()
draw_hud()
```

Keep fractional movement in game state and round at the drawing boundary.
Following, smoothing, world bounds and collision coordinates belong to game
logic. No zoom, rotation or camera stack is introduced. Earlier engine versions
lack these functions. See the [camera demo](../../examples/camera-demo/README.md).

## Sprite tint and opacity

`sugarbee.sprite_tint(r, g, b [, a])` takes exactly three or four integer-valued
channels in **0..255**. Omitted alpha is 255, even if the previous tint was
translucent. Explicit `nil` is not an omitted channel. Integer-valued floats
are accepted; negative, fractional or out-of-range channels raise a Lua error
before changing state. Unlike the older primitive color conversion, these
channels are checked rather than wrapped. `sprite_tint_reset()` takes no
arguments. Both functions return no values.

One active sprite tint affects subsequent `sprite`, `sprite_region`,
`sprite_rotated` and `sprite_region_rotated` calls,
including PNG and RGBA textures, cropped/scaled/flipped regions, camera offsets
and clipping. Calls replace all four channels; they do not accumulate.
The texture's pixel data is unchanged, so one sheet can be reused in many colors.

RGB channels multiply the source channels by tint/255. White takes the selected
color, gray becomes a darker shade, and black stays black. This is not arbitrary
palette replacement, nor can it brighten a source channel above its original
value. Draw opacity likewise multiplies source alpha by a/255 before ordinary
alpha blending: zero is invisible and 255 preserves the original transparency.
Backend rounding can differ by small amounts for intermediate channel values.

`print`, `rect`, `line`, `circle`, `circle_fill`, `triangle_fill` and `cls` keep their own color behavior. Clearing does not
reset tint; camera and clip setters/resets also leave tint unchanged. Explicit
tint reset selects `(255,255,255,255)`. Every game frame resets tint before
`draw()`, and host presentation, error screens and cartridge cleanup isolate it.
Set tint inside `draw()`; `init()`/`update()` settings do not persist into drawing.
Existing cartridges default to their original colors and transparency.

```lua
sugarbee.sprite_tint(255, 100, 80)
sugarbee.sprite(glyph_sheet, 40, 80)
sugarbee.sprite_tint(80, 160, 255, 128)
sugarbee.sprite(glyph_sheet, 120, 80)
sugarbee.sprite_tint_reset()
sugarbee.sprite(glyph_sheet, 200, 80)
```

Texture modulation/render failures use the existing sprite drawing error path.
Earlier engines lack these tint functions. See the
[tint demo and acceptance checklist](../../examples/tint-demo/README.md).

## Line primitive

`sugarbee.line(x1, y1, x2, y2 [, r, g, b])` takes four through seven arguments.
Endpoints must be integer-valued and fit the supported signed 32-bit range.
Each omitted trailing color channel defaults to 255; supplied channels must be
integer-valued and in 0..255. Explicit `nil` is invalid. Integer-valued floats
are accepted. Invalid arguments raise Lua errors before drawing; renderer
failures raise an error. Success returns no values. Existing primitive color
conversion is unchanged; this new API uses checked colors like sprite tint.

Both endpoints are included. Identical endpoints draw a single pixel. Lines
are opaque, one **logical** pixel wide, without antialiasing. Camera translation
and the screen-space clip apply. Sprite tint has no effect. Later drawing
covers earlier drawing as usual. The call adds no persistent drawing state.

The entire supported coordinate range is allowed. Safe wide arithmetic and
skipping directly to the visible span keep work bounded by the viewport's
width or height, not the endpoint distance. Empty clips and trivially offscreen
lines return without drawing. A line can cross the screen with both endpoints
outside it. This bounds individual calls, not the number of calls in a frame.

### Defined pixel pattern

The axis with the larger absolute endpoint difference is the major axis (x
wins ties). Endpoints are ordered in the increasing major-axis direction.
At every integer position along that axis, the minor coordinate is rounded to
the nearest pixel, with exact halfway ties toward the other endpoint's minor
coordinate. This fixes the same raster pattern on supported desktop/Pi backends
and makes endpoint reversal pixel-identical. For example, `(0,0)` to `(4,2)`
draws `(0,0), (1,1), (2,1), (3,2), (4,2)` in either argument order.

Clipping selects pixels from that original pattern. It does not round new
endpoints and restart the line, so moving a clip cannot shift the remaining
pixels. The implementation evaluates the integer raster phase directly and
submits bounded batches of pixels to the renderer.

```lua
sugarbee.line(20, 30, 300, 120)              -- white diagonal
sugarbee.line(20, 140, 300, 140, 255, 80, 60) -- colored horizontal
sugarbee.line(40, 160, 40, 160)              -- one pixel
```

Thickness, line alpha, antialiasing, dashed lines, connected-path helpers and
filled triangles are outside this checkpoint. Earlier engines lack `line`.
See the [line demo](../../examples/line-demo/README.md).


## Circle primitives

`sugarbee.circle(cx, cy, radius [, r, g, b])` draws an outline;
`sugarbee.circle_fill(cx, cy, radius [, r, g, b])` draws a solid disk.
Both take three through six arguments and return no values.

The center and radius must be integer-valued and within the native signed
32-bit range on supported targets. The radius must be nonnegative; zero draws
one pixel. Each omitted trailing RGB channel defaults to 255. Supplied channels
must be integers in 0..255; explicit `nil` is invalid. All arguments are checked
before drawing; invalid arguments and renderer failures raise Lua errors.

Both calls use the same integer pixel rule: a pixel belongs to the disk when
`dx*dx + dy*dy <= radius*radius`, measured from its center. The outline contains
disk pixels with at least one of their four neighbors (left, right, above, below)
outside the disk. This is a symmetric one-pixel boundary layer, entirely within
the fill. Drawing an outline over the corresponding fill replaces only that
boundary. Tiny circles are deliberately pixelated: radius 0 is a point, and
radius 1 is a five-pixel cross for the fill, with four outer pixels for the outline.

Camera translation applies to the center. Screen-space clipping hides pixels
without changing the circle or adding an outline along the clip edge. Offscreen
circles and empty clips succeed without drawing. Circles are opaque and ignore
sprite tint; later drawing replaces earlier pixels.

The renderer uses wide arithmetic and exact integer square roots, with no
floating-point rounding dependency. It visits only visible rows and submits
clipped horizontal spans. Work is bounded by viewport/clip height, even for
radius 2147483647. A huge disk can cover the viewport while its outline is
entirely offscreen; such an outline correctly draws nothing.

```lua
sugarbee.circle_fill(160, 120, 40, 60, 120, 200)
sugarbee.circle(160, 120, 40) -- white boundary over the same disk
sugarbee.circle(20, 20, 0)   -- one white pixel
```

No arcs, wedges, ellipses, adjustable thickness, antialiasing or primitive alpha
are included. Earlier engines lack these calls.
See the [circle demo](../../examples/circle-demo/README.md).


## Filled triangles

`sugarbee.triangle_fill(x1, y1, x2, y2, x3, y3 [, r, g, b])` takes six
through nine arguments and returns no values. Vertex coordinates must be
integer-valued within the signed32 native range. Each omitted trailing RGB
channel defaults to 255; supplied channels must be integers in 0..255.
Explicit nil is invalid. All arguments are validated before drawing; invalid
arguments or renderer failures raise Lua errors.

Integer vertices describe pixel boundaries. Coverage is evaluated at pixel
centers `(x + 0.5, y + 0.5)`. Centers exactly on an edge are included for
top/left boundaries and excluded for bottom/right boundaries (the top-left
rule). All six vertex orders produce identical coverage. A zero-area triangle,
including repeated vertices or three collinear points, draws nothing.

Two adjoining triangles with exactly matching shared endpoints assign each
shared-edge pixel to exactly one triangle, without gaps or double drawing.
Splitting the rectangle from (10,10) to (30,20) along either diagonal covers
exactly the same pixels as `sugarbee.rect(10,10,20,10,r,g,b,true)`.
This guarantee concerns shared edges, not deliberately overlapping shapes or
approximately matching vertices.

Camera translation applies to all vertices. Screen-space clipping only hides
pixels; it does not recalculate the shape from rounded clipped vertices.
Triangles are opaque and ignore sprite tint. Later draws replace earlier
pixels wherever shapes overlap.

The native renderer visits only visible rows, using exact rational edge
intersections and clipped horizontal spans. Quotient/remainder arithmetic
avoids overflowing full-range cross products or doubled products, using
portable signed/unsigned 64-bit C arithmetic. No floating-point coverage,
radius/edge-length loop or allocation is needed.

```lua
sugarbee.triangle_fill(10, 10, 30, 10, 30, 20, 255, 180, 60)
sugarbee.triangle_fill(10, 10, 30, 20, 10, 20, 60, 140, 240)
```

Existing `line` calls can draw visible outlines, but their inclusive endpoint
rasterization does not promise an exact match to the fill boundary. No separate
outline API, general polygons, gradients, textures, alpha or antialiasing is
included. Earlier engines lack this function.
See the [triangle demo](../../examples/triangle-demo/README.md).

## Sprite rotation

The separate `sprite_rotated` and `sprite_region_rotated` calls preserve existing
sprite APIs. Both return zero Lua results. Invalid handles, arguments, source
regions, size limits or failed drawing raise a Lua error. Region source bounds
must lie entirely within the texture and all source/destination sizes must be
positive. The rotated-region call requires both destination dimensions; it has
no flip parameters in this checkpoint.

- `x, y` denote the **unrotated destination top-left**, in signed 32-bit integer
  logical coordinates. Region coordinates and sizes use the same integer rules.
- `angle` is finite clockwise degrees, including fractional values. Zero retains
  the artwork's original orientation; negative angles and values beyond 360 wrap.
  Very large finite values are accepted but already-lost floating-point precision
  cannot be recovered. Nonfinite angles are errors.
- With no pivot arguments, the pivot is the exact destination center, including
  half-pixel centers for odd dimensions. If supplied, **both** coordinates must
  be finite numbers within -2147483648..2147483647. Fractional and outside pivots
  are allowed. Explicit nil pivots are invalid; omit both to select the center.
- Pivots are measured from the unrotated top-left in **destination pixels**.
  A 16×16 region drawn at 64×64 defaults to pivot `(32,32)`, not `(8,8)`.
- Destination limits apply before drawing/culling, including zero-angle and
  offscreen calls: **4096 per axis** and **4,194,304 total pixels**. Whole sprites
  use their texture dimensions. These limits apply only to the new rotation APIs.
- Existing sprite tint, alpha blending, camera and destination clipping apply.
  Rotation adds no persistent state; subsequent drawing and host UI are unrotated.
  A source rectangle outside the viewport may rotate into view and is not culled
  solely by its original position.
- Sampling remains nearest-neighbor. Arbitrary-angle pixel art naturally has
  stair-stepping; fractional edge coverage can vary between SDL backends.
  A normalized zero angle uses the existing sprite path exactly.

```lua
-- Whole texture, rotating around its center.
sugarbee.sprite_rotated(ship, x, y, heading + artwork_offset)
-- 16x16 sheet region, scaled to 64x64, rotating around its top-left.
sugarbee.sprite_region_rotated(sheet, x, y, 16, 0, 16, 16, 64, 64, angle, 0, 0)
```

The SDL backend uses `SDL_RenderCopyExF`, requiring SDL **2.0.12 or later**.
Custom pivots are converted into an equivalent shifted-center rotation; rotated
bounds are checked in double precision before conversion to SDL coordinates.
The destination limits bound work/temporary image sizes in the software fallback;
they are not a recommended draw budget or a constant-memory rendering promise.
Native functions `sb_graphics_sprite_rotated` and
`sb_graphics_sprite_region_rotated` return false on invalid input or rendering
failure, true on success (including a valid fully offscreen draw). `custom_pivot`
selects whether to use the supplied pivot doubles or the exact center.

See [rotation demo and acceptance checks](../../examples/rotation-demo/README.md).

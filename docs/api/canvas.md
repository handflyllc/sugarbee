# Cartridge canvas and presentation

Implemented in the logical-canvas checkpoint based on `36ba975`.
The user confirmed all desktop checks passed on 2026-11-18. Pi acceptance is pending.

## Declaration

Put optional `console.json` at the root of the cartridge source directory;
`sbee-pack` includes it like any other asset. The host validates it before
executing `main.lua` or `init()`:

```json
{"video":{"width":300,"height":200}}
```

An absent file or `{}` selects the existing 1280×720 default. If `video` is
present, it must be an object containing both `width` and `height`. Values must
be positive JSON integer literals (not strings, booleans, fractions or exponent
notation) and match one exact supported pair:

| Canvas | Scale on 1280×720 output | Displayed image | Borders (left/right, top/bottom) |
| --- | --- | --- | --- |
| 300×200 | 3× | 900×600 | 190, 60 |
| 320×240 | 3× | 960×720 | 160, 0 |
| 640×360 | 2× | 1280×720 | 0, 0 |
| 960×720 | 1× | 960×720 | 160, 0 |
| 1280×720 | 1× | 1280×720 | 0, 0 |

The uncompressed file limit is **4096 bytes**, checked before decompression.
Strict JSON applies: no comments or trailing commas. Unknown fields, duplicate
fields, partial dimensions, unsupported pairs, malformed JSON and asset read/CRC
failures are errors, not defaults. Errors enter Guru with details in the console;
cartridge Lua does not execute when configuration validation fails.
Only `video` is recognized at the root; only `width` and `height` within it.
There is no schema/version field or cartridge scaling preference in this checkpoint.
The `.vcart` binary header/ZIP format is unchanged. Older hosts do not implement
this declaration or the dimension-query API.

## Game coordinates

The canvas remains fixed until the cartridge exits. All existing graphics
coordinates, camera translation and clip bounds use its logical pixels.
Draw in `draw()` as before; this declaration does not change callback timing.

- `sugarbee.width()` returns one integer: the game canvas width.
- `sugarbee.height()` returns one integer: the game canvas height.

Both accept **zero arguments**; extra arguments, including explicit `nil`, raise
Lua errors. Queries work during top-level code, init, update, draw and shutdown;
they do not report the window, host menu or physical display dimensions.

```lua
local width, height = sugarbee.width(), sugarbee.height()
function draw()
    sugarbee.cls(12, 18, 30)
    sugarbee.rect(0, 0, width, height, 255, 255, 255, false)
end
```

## Host presentation

The host chooses the largest integer enlargement that fits the **renderer output**,
uses nearest-neighbor sampling and centers the image on black. It never stretches
it to fill a different aspect ratio or crops its edges. Spare odd pixels go to
the right/bottom border. If even 1× cannot fit, the image is proportionally
reduced; the unconstrained dimension rounds down to an integer pixel, with a
minimum of one pixel. Such downscaling loses source pixels and can introduce
subpixel aspect-ratio rounding. It is a fit fallback, not pixel-perfect rendering.

The declaration does not change window size or request a display mode. On a
1280×720 window, a 960×720 game therefore retains 160-pixel side borders.
Host launcher, Pause and Guru use a separate 1280×720 logical canvas, fitted to
the output by the host renderer. Pause displays the retained game image beneath
the host overlay; overlays never alter that retained image. Resume redraws the
game normally. Exit, failed startup and relaunch restore host state and apply
the next cartridge's own declaration/default; camera, clip and tint do not leak.

Options menus, game settings manifests, persistence, runtime resolution changes
and console updates are outside this checkpoint.

## Verification

Native configuration checks cover all pairs, defaults, strict types, duplicates,
unknown fields, malformed input and both sides of the 4 KiB boundary. Packed
fixtures exercise missing/oversized/invalid declarations before Lua, dimension
queries through lifecycle callbacks, startup/draw errors and recovery.
SDL dummy-renderer pixel checks cover all supported pairs, corner pixels,
nearest-neighbor enlargement, black borders, non-multiple output, both downscale
constraints and separation of host overlays from the retained game image.

The Lua smoke runner uses explicit trusted `scenario.canvas` metadata for
cartridges containing `console.json`; it does not parse or validate that JSON.
See [smoke-runner limitations](../../tests/smoke-runner.md) and the
[canvas demo and manual checks](../../examples/canvas-demo/README.md).

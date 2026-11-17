# Camera demo

This checkpoint adds integer camera positions to the existing graphics API.
See the [camera contract](../../docs/api/graphics.md#camera-position).
The labeled world scrolls horizontally and vertically through a fixed panel;
its border, title and HUD remain stationary. The clip stays in screen coordinates.

```sh
cmake --build build -j4
./build/sbee-pack -i examples/camera-demo -o build/camera-demo.vcart -t "Camera Demo"
./build/sugarbee build/camera-demo.vcart
```

## Manual acceptance (desktop, then Pi)

1. Labeled cells, gold sprites and outlines scroll together in both directions,
   stopping cleanly at the panel edges. Title, border and HUD remain fixed.
2. Start/Enter opens a complete pause menu. Resume, Exit Game and relaunch work;
   the camera/clip deliberately left by draw and shutdown never hide host screens.
3. Y/I deliberately sets an extreme camera and empty clip before raising a Lua
   error. Guru and its diagnostic remain visible; B/K recovers, A/J relaunches.
4. Run Pong and the clip demo: drawing and pause/resume remain normal.
5. Exit to system returns normally.

Run CTest serially on desktop and Pi. No audio comparison is required. This demo
is excluded from random-input smoke runs because Y intentionally raises an error.

## Validation

Native pixel comparisons cover translated rectangles, outlines, multiline text,
native sprites and scaled/flipped regions, negative/extreme cameras, replacement,
clear preserving state, screen-space clipping, offscreen culling and lifecycle
resets. Production Lua tests cover argument validation, no return values,
failed calls preserving the camera and Guru isolation.
Desktop: warning-free build, **143/143 serial CTests passed**; after the final
rectangle compatibility adjustment, all ten targeted graphics/runner checks
passed again. Native camera and production Lua/graphics UBSan checks passed.
On 2026-11-17, the user confirmed all desktop manual checks passed: scrolling
and fixed HUD, pause/resume/exit/relaunch, Guru recovery, Pong and clip-demo
regressions, and Exit to system. These results cover the camera checkpoint
built on `2f550ec`. Pi validation remains pending.

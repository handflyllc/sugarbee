# Filled triangle demo

Opaque triangles with pixel-center sampling and top-left shared-edge ownership.
See the [API contract](../../docs/api/graphics.md#filled-triangles).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/triangle-demo -o build/triangle-demo.vcart -t "Triangle Demo"
./build/sugarbee build/triangle-demo.vcart
```

## Manual acceptance (desktop, then Pi)

1. The two split rectangles have clean diagonal seams, no gaps, and the same
   dimensions as the green reference rectangle. Colors and geometry stay fixed
   as the order label alternates: both draw order and vertex order reverse.
2. The pink triangle remains fixed too. Diagonal edges are deliberately pixelated.
3. The zero-area marker remains empty: no red line appears.
4. The full-range panel stays solid green and the game remains responsive.
5. Cyan triangles scroll inside the lower panel, with clean clipping and no
   spill outside its border. They remain visible despite zero sprite opacity.
6. Pause/resume, Exit Game/relaunch and Exit to system work normally.
   Pong and circle-demo still render normally.

Run CTest serially on desktop and Pi. No new audio comparison is required.

## Validation

Native checks compare 200 fixed/seeded cases against an independent edge-function
oracle, with all six vertex orders and full/custom clipping. Both diagonal splits
of a rectangle match the existing rectangle fill; different colors and reversed
submission order detect shared-edge overlap. Full-range cases include analytic
diagonals and eight golden coverage hashes from arbitrary-precision edge
equations. Extreme cameras, degenerate triangles, empty clips and sprite-tint
isolation are covered. The timeout guards against walking offscreen geometry.

Production Lua tests check argument counts, native coordinate ranges, strict
colors, default channels, nil rejection, no returns and invalid calls drawing
nothing. The smoke runner validates arguments but does not render pixels.

Desktop: warning-free build, **147/147 serial CTests passed**, and native
triangle plus production Lua/graphics UBSan checks passed. The packaged demo
passed a 180-frame simulated smoke run and a two-second dummy-driver host launch.
On 2026-11-17, the user requested committing this checkpoint and deferring Pi
validation until after a break. No explicit desktop manual-check results have
been recorded. Pi validation remains pending.

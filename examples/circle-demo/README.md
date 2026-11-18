# Circle primitives demo

Opaque circle outlines and filled disks with matching pixel boundaries,
camera offsets and screen-space clipping.
See the [circle contract](../../docs/api/graphics.md#circle-primitives).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/circle-demo -o build/circle-demo.vcart -t "Circle Demo"
./build/sugarbee build/circle-demo.vcart
```

## Manual acceptance (desktop, then Pi)

1. The top row shows an outline, a solid blue disk, and a blue disk with a yellow
   outline. Shapes are symmetric; the yellow boundary neatly replaces the outer
   layer of the fill without leaving gaps.
2. The small-radius rows show radii 0 through 8, outlines above and fills below.
   The first item in each row is one pixel. Small circles look deliberately
   pixelated; radius 1 is a cross-shaped fill with four outline pixels.
3. The right-hand huge-disk panel is solid green with no red. Its radius is
   2147483647, and its red outline is entirely offscreen. The game remains responsive.
4. Blue disks with yellow outlines scroll through the lower panel. They are
   clipped cleanly without spilling outside the border or gaining artificial
   yellow edges along the clip. Sprite tint is zero opacity during this drawing,
   so these opaque circles must remain visible.
5. Start/Enter pause, resume, Exit Game and relaunch work normally.
6. Pong and line-demo still render normally; Exit to system returns normally.

Run CTest serially on desktop and Pi. No new audio comparison is required.

## Validation

Native tests compare each pixel against an independent disk-membership oracle
for every radius 0 through 65, three center positions, both outline/fill calls
and full/custom clips. Additional cases cover maximum radii, tangencies,
extreme camera translation, empty clips, tint isolation, colors, invalid native
arguments and drawing order. The timeout guards against walking huge radii.

Production Lua tests cover arity, integer/radius/color validation, zero radius,
omitted channels, explicit nil rejection, no return values and invalid calls
drawing nothing. Smoke-runner stubs check arguments without simulating pixels.

Desktop: targeted graphics/runner checks and native circle plus production
Lua/graphics UBSan checks passed. Warning-free build. The demo passed a
180-frame simulated smoke run and a two-second dummy-driver host launch.
**146/146 serial CTests passed**. On 2026-11-17, the user confirmed all desktop
manual checks passed: circle symmetry, fill/outline agreement, small radii,
huge-disk behavior, clipped scrolling, tint isolation, pause/resume/exit/relaunch,
Pong/line-demo regressions and Exit to system. These results cover the circle
checkpoint built on `c2524c0`. Desktop acceptance is complete; Pi validation
is pending.

# Line primitive demo

One-pixel opaque lines with inclusive endpoints, camera offsets and screen-space
clipping. See the [line contract](../../docs/api/graphics.md#line-primitive).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/line-demo -o build/line-demo.vcart -t "Line Demo"
./build/sugarbee build/line-demo.vcart
```

## Manual acceptance (desktop, then Pi)

1. The upper fan and horizontal/vertical lines remain fixed as the endpoint-order
   label changes every second. Diagonals are crisp, without changing pixels.
2. Each of the four outlined marker boxes contains a single white center pixel.
3. The lower wireframe scrolls through a fixed clipped panel. Nothing spills
   across its border. A white horizontal line spans the panel even though its
   endpoints are at the signed integer extremes. Motion remains responsive.
4. The lower lines retain their colors while sprite tint is set to zero opacity.
5. Start/Enter pause, resume, Exit Game and relaunch work normally.
6. Pong and tint-demo still render normally; Exit to system returns normally.

Run CTest serially on desktop and Pi. No new audio comparison is required.

## Validation

Native tests compare 600 fixed/seeded small lines with a full-length incremental
reference raster, in both endpoint orders, with and without clipping. Additional
cases cover extreme crossings, camera translation, one-point lines, empty clips,
offscreen lines, strict RGB output, sprite-tint isolation and submission order.
The test timeout detects attempts to walk billions of offscreen pixels.
Production Lua tests check arity, coordinate/color ranges, default channels,
no return values and invalid calls drawing nothing.
Desktop: warning-free build, **145/145 serial CTests passed**, and native line
plus production Lua/graphics UBSan checks passed. The demo passed a 180-frame
simulated smoke run and a two-second dummy-driver host launch. These do not
replace visual acceptance.
On 2026-11-17, the user confirmed all desktop manual checks passed: endpoint
reversal, single pixels, clipped scrolling and extreme-coordinate lines, tint
isolation, pause/resume/exit/relaunch, Pong/tint-demo regressions and Exit to
system. These results cover the line checkpoint built on `ba796d3`.
Pi: on 2026-11-17, the user confirmed all checks passed at `0357700`, including
**145/145 serial tests**, endpoint reversal, single pixels, clipped scrolling,
extreme-coordinate lines, tint isolation, lifecycle, Pong/tint-demo regressions
and Exit to system. Desktop and Pi acceptance are complete.

Visual observation: the user reported considerably more washed-out colors on
the Pi display, but judged the output acceptable. The display and the fineness
of the one-pixel lines were suggested as possible explanations; the cause has
not been established. No color correction or renderer change was made.

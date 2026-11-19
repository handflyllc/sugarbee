# Canvas demo

A 300×200 cartridge with a one-pixel border, alternating one-pixel bars,
a circle and a clipped moving square. Geometry uses the new dimension queries.
See the [canvas contract](../../docs/api/canvas.md).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/canvas-demo -o build/canvas-demo.vcart -t "Canvas Demo"
./build/sugarbee build/canvas-demo.vcart
```

## Desktop acceptance, then Pi

1. On a 1280×720 output, the image is 900×600 with black borders: 190 pixels
   at either side and 60 above/below. All four white edges are visible;
   the alternating bars have equal widths and the circle is round.
2. The label reads `300 x 200 canvas`. The square moves inside the lower
   clipped panel; the horizontal line stays inside its left/right bounds.
3. Pause with Enter/Start. The host menu stays its usual readable size.
   Resume: no overlay remnants, image distortion or clip/camera leakage.
4. Press K/B while playing to trigger the deliberate Guru error. Guru remains
   readable. Return to host, relaunch, and check the canvas is unchanged.
5. Exit Game/relaunch, Pong and Pac-Man, and Exit to system behave normally.

Optional comparison: edit the demo's `console.json` to another supported pair
and repackage. At 960×720 the host window stays 1280×720 with side borders;
at 320×240 the image enlarges 3× to the same 960×720 displayed area. Restore
300×200 afterward. Native tests cover every pair without changing this demo.

Run CTest serially:

```sh
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

The smoke model requires `tests/canvas_scenario.lua` to supply dimensions;
its one-frame check avoids the runner's randomized B input intentionally
triggering the demo's Guru. It is not a visual or native configuration test.

## Validation

Desktop: **162/162 serial CTests passed**. Configuration and real-renderer
lifecycle checks also passed UBSan. The packaged demo stayed running during a
two-second dummy-driver launch without reporting a Guru error; the harness
terminated it. On 2026-11-18, the user confirmed all desktop checks passed:
canvas appearance, Pause/resume, intentional Guru recovery, return/relaunch,
Pong/Pac-Man regressions and Exit to system.
On 2026-11-18, the user confirmed Pi acceptance at `f84ffbd`: **162/162 tests**,
Pong regression and all remaining requested visual, Pause/Guru, lifecycle,
Pac-Man and Exit to system checks passed. Desktop and Pi acceptance are complete.

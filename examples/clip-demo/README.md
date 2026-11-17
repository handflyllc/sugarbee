# Clip rectangle demo

Base: `16f408a`. This checkpoint adds `sugarbee.clip(x,y,w,h)` and
`sugarbee.clip_reset()`; see the [graphics contract](../../docs/api/graphics.md#clip-rectangles).
It does not change logical resolution or introduce a camera/clip stack.

```sh
cmake --build build -j4
./build/sbee-pack -i examples/clip-demo -o build/clip-demo.vcart -t "Clip Demo"
./build/sugarbee build/clip-demo.vcart
```

The central 960×400 panel clips a moving gold sprite, scrolling text and a pink
rectangle. A white border outlines the panel. The title is drawn before setting
a clip, and the footer after an explicit reset. Each draw then deliberately
leaves an empty clip: the full-screen red rectangle must never be visible, and
the next frame's title must still render.

## Manual acceptance (desktop, then Pi)

1. Gold sprite/text stop cleanly at all panel edges. The pink rectangle stops at
   the bottom edge; neither spills into the title/footer area.
2. Title, border and footer remain visible while the animation moves. No red
   full-screen flash appears. This exercises explicit and automatic reset.
3. Start/Enter opens a complete pause menu even though draw left an empty clip.
   Resume, Exit Game and relaunch work normally; title/footer reappear.
4. Y/I deliberately sets an empty clip in update and raises a Lua error. The Guru
   screen and diagnostic must remain visible. B/K recovers; A/J relaunches. This
   is expected, not a failed acceptance check. Console output retains the error.
5. Run an older cartridge (Pong or Pac-Man), check graphics and pause/lifecycle,
   then Exit to system. Existing drawing must remain unchanged.

On Pi use the committed desktop revision and run CTest serially before the same
visual checklist. No new audio listening comparison is needed for this change.
The demo is excluded from random-input smoke runs because Y intentionally errors.

## Automated verification

`graphics_clip` checks readback pixels for filled/outline rectangles, sprites,
scaled/flipped source regions, text, half-open edges, replacement, offscreen/empty
clips, extreme integer coordinates, clear preserving the clip, reset, retained
frame copying, host overlay drawing, next-frame reset and session texture cleanup.
The production Lua-binding test checks arity/ranges/negative dimensions and that
invalid arguments preserve the previous clip; it also invokes the real Guru
renderer with an empty clip. Smoke-runner tests cover the new validation stubs.

Desktop: warning-free build, **142/142 serial CTests passed**, and UBSan
checks passed for the native clip and production Lua-binding tests. On
2026-11-17, the user confirmed all requested desktop visual, Guru and lifecycle
checks passed, with Pong rendering normally as the existing-cartridge check.
These results cover the clip checkpoint built on `16f408a`.
Pi manual acceptance remains pending. SDL dummy pixel tests are not a substitute
for accelerated-renderer visual checks.

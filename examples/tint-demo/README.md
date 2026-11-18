# Sprite tint demo

One original two-glyph sheet is reused in multiple colors and opacities.
White pixels take the tint; gray pixels remain darker; transparent holes remain
transparent. See the [API contract](../../docs/api/graphics.md#sprite-tint-and-opacity).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/tint-demo -o build/tint-demo.vcart -t "Sprite Tint Demo"
./build/sugarbee build/tint-demo.vcart
```

## Manual acceptance (desktop, then Pi)

1. The top glyphs stay white/gray across frames. The next row shows red, green,
   blue and gold copies, followed by an original white/gray copy after reset.
   Glyph holes stay transparent and the diamond's lower half remains darker.
2. The blue pair fades over a checkerboard; text and rectangles remain opaque.
3. Colored, flipped diamonds scroll inside the lower panel without spilling
   across its edges. Its border and the HUD remain fixed.
4. Start/Enter pause, resume, Exit Game and relaunch work normally, including
   the retained game image. Draw/shutdown deliberately leave tint at opacity zero.
5. Y/I intentionally raises Guru after selecting opacity zero. The error screen
   remains visible; B/K recovers and A/J relaunches with original colors restored.
6. Pong and camera-demo still render normally. Exit to system returns normally.

Run CTest serially on both machines. No new audio comparison is required.
This demo is excluded from random-input smoke runs because Y intentionally errors.

## Validation

Native pixel tests cover white/gray/black/color modulation, partial and zero
source alpha, multiplied draw opacity over a background, same/shared texture
reuse, primitive/text isolation, crop/scale/flip/camera/clip composition, invalid
handles/regions and explicit/frame/host/session resets. Production Lua tests
check arity, channels, atomic rejection, default opacity and Guru isolation.
Desktop: warning-free build, **144/144 serial CTests passed**, and native tint
plus production Lua/graphics UBSan checks passed. A two-second dummy-driver
cartridge launch completed without Guru; this is not visual acceptance.
On 2026-11-17, the user confirmed all desktop manual checks passed: colors and
transparency, fading, clipped scrolling, pause/resume/exit/relaunch, Guru recovery,
Pong/camera-demo regressions and Exit to system. These results cover the tint
checkpoint built on `0d36c02`. Pi validation remains pending.

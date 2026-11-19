# Rectangle and text opacity

Per-call alpha defaults to 255 (opaque); 0 is invisible and intermediate values
blend over previously drawn pixels. A/J cycles 0, 64, 128, 192 and 255, starting
at 128. The demo exercises filled and outline rectangles, scaled multiline text,
clip/camera interaction and subsequent opaque drawing.

```sh
./build/sbee-pack -i examples/opacity-demo -o build/opacity-demo.vcart -t "Opacity Demo" --uuid 67495338-a94f-4e22-bc16-4506a68a9043
./build/sugarbee build/opacity-demo.vcart
```

Desktop and Pi manual acceptance:

1. Cycle A/J. The checkerboard shows through the fading black panel, colored
   outline and text. At 0 these disappear; at 255 they are opaque. The panel's
   label and bottom footer remain opaque throughout.
2. Outline corners have the same intensity as the edges. Multiline scaled text
   keeps its existing shapes and spacing. The lower-right sample stays clipped.
3. Pause/resume at alpha 0 and 128: host menu remains readable and opaque.
   Exit/relaunch resets the demo to 128; normal Exit to system works.
4. Launch Pong and confirm unchanged rendering.

Implementation baseline: dd650c5. Desktop automated validation passed
on 2026-11-19: 180/180 serial tests and native pixel UBSan checks passed.
The user confirmed all desktop manual checks passed on 2026-11-19: opacity
levels, uniform corners, text/clipping, opaque host menus, relaunch reset, Pong
regression and Exit to system. Pi validation remains pending. The native pixel test covers blending,
zero opacity, border coverage (including 1-pixel widths/heights), scaled multiline
text, wide arithmetic, clip/camera and subsequent opaque host drawing. Production
Lua checks cover alpha validation and return counts. The smoke runner models
argument checks only; it does not validate rendered pixels.

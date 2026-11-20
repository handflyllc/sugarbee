# Sprite rotation demo

Whole-sprite and scaled sheet-region rotation, centered and external pivots,
tint/opacity, camera and clipping. Text and the lower-left reference remain
unrotated. White crosses mark fixed rotation pivots. Artwork points right at 0°.

```sh
./build/sbee-pack -i examples/rotation-demo -o build/rotation-demo.vcart -t "Rotation Demo" --uuid 8b172fb6-918d-4d55-9d0c-f675432bc216
./build/sugarbee build/rotation-demo.vcart
```

- A/J advances by 45°.
- B/K toggles continuous clockwise rotation with fractional angles.
- Enter/Start opens the host pause menu.

Desktop and Pi acceptance:

1. Step through 0°, 90°, 180°, 270°: arrows point right, down, left, up.
   Centered arrows rotate around their crosses. The external-pivot arrow orbits
   its cross. The scaled sheet region remains orange, with no neighboring frame.
2. Toggle animation. Check fractional angles, tint/opacity and the clipped sample.
   Pixel-art stair-stepping is expected; the reference sprite and text stay fixed.
3. Pause/resume keeps host menus unrotated and restores the demo. Exit/relaunch
   starts at 0° with animation off. Normal Exit to system works.
4. Pong and the opacity demo still render normally.

Implementation baseline: `1d6e289`. Desktop automated validation passed on
2026-11-19: **184/184 serial tests**, plus native pixel UBSan checks.
The user confirmed all desktop validation checks passed on 2026-11-19:
quarter-turn orientation, center/external pivots, continuous rotation, tint/opacity,
clipping, Pause/resume, relaunch reset, Pong/opacity-demo regressions and Exit
to system. Pi acceptance remains pending.

Native pixel checks cover orientation at quarter turns, negative/wrapped angles,
zero-angle parity, odd-size center equivalence, external pivot visibility,
scaled source regions, tint/opacity, clip/camera, stale handles and extreme inputs.
Lua and smoke-model checks share an argument-contract regression, including size
boundaries and invalid calls. Native pixel checks also pass UBSan. Smoke checks
model argument behavior only, not rendering. Subpixel edge coverage can differ
between SDL backends; exact cross-backend pixel identity is not promised.

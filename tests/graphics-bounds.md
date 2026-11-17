# Graphics integer and text bounds repair

Base: `e005b5d`. Repairs review findings R1/R2 from
[the friction review](../docs/development/friction-review-2026-11-16.md).
Desktop acceptance is complete: the user confirmed all three visual/lifecycle
checks passed on 2026-11-17, against the working changes based on `e005b5d`.
Together with 131/131 serial tests and UBSan checks, this completes desktop
validation. Runtime repairs were committed and pushed as `d2ee33c`.

Pi acceptance is complete: on 2026-11-17 the user confirmed all requested checks
passed at `d2ee33c`: 131/131 serial tests, sprite-sheet movement/scaling/flips/text,
game sprites/score text, pause/resume/exit/relaunch and Exit to system. These are
user-reported hardware results, not independently executed remote checks.

## Scope

- Rectangle coordinates/dimensions, text positions/scale and whole-sprite
  positions now use the same checked Lua-integer conversion as sprite regions.
  Unsupported values raise argument errors before drawing. Fractional values
  still fail; integer-valued floats, normal negative positions and defaults work.
- Player selection checks the full Lua integer before narrowing. Every integer
  outside 0–3 returns false, including values that previously wrapped to player 0.
- Native text uses wide cursor/glyph arithmetic and clips glyph pixels before
  narrowing to SDL rectangles. Horizontally offscreen rows still scan for newline;
  downward movement stops once later rows cannot be visible. Cursor arithmetic
  stays bounded regardless of string length. Huge scales can still intersect
  the viewport and render their visible pixels safely.
- Colors, texture/source-region semantics, audio, formats and instruction budgets
  are unchanged. This adds no public clip, camera or tint API. The smoke runner
  remains a separate follow-up.

## Automated evidence

Build and run serially:

```sh
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

`lua_graphics_bounds` exercises real host bindings and checks out-of-range
coordinates/dimensions/scale, fractional rejection, valid integer-valued floats,
negative positions, extreme text, and invalid player indices while player 0
actually has a held/pressed key. `font_smoke` retains pixel checks for ordinary
font output and adds extreme coordinates/scales, visible portions of huge glyphs,
negative positions and newline recovery after a long row.

Both tests passed with the production bindings/backend compiled using
`-fsanitize=undefined -fno-sanitize-recover=undefined`, SDL dummy video. Running
the new font test against the old backend fails with signed overflow; running
the new binding test against the old host fails its invalid-input assertion.
Temporary sanitizer/regression probes are in `/tmp/sugarbee-bounds-validation`;
they are not build dependencies. Normal build produced no compiler warnings.
Full desktop serial suite: **131/131 passed**, exit 0. This includes the new
binding regression; it is not Pi acceptance.

## Manual acceptance checklist (completed on desktop and Pi)

Use the same source revision on both targets. On Pi, rebuild and run the full
serial suite before the visual checks. Existing packaged cartridges remain valid.

1. Launch the sprite-sheet demo: movement, crop/scale/flip and text look unchanged.
2. Play Pong or Pac-Man briefly; scores/text and sprites render normally.
3. Open the host pause menu, resume, exit and relaunch. Text/overlays remain intact.

No new listening comparison is required by these graphics changes. Dummy-driver
pixel checks do not establish accelerated-renderer or Pi visual acceptance.
The previous rare OS freezes remain undiagnosed; these fixes do not establish
that their cause has been found.

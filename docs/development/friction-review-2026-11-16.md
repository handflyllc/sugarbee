# Friction reconciliation and focused review — 2026-11-16

## Scope and evidence

Reviewed SugarBee `7085043` (runtime `4d7ca11`) on Debian x86_64. This checkpoint
changes documentation only. It does not authorize feature implementation.
The [original friction report](friction-report-2026-11-16.md) is preserved as
received, including its historical claims; the dispositions below supersede
those claims for planning. Its five game sources were not reviewed or rerun.
Reported desktop/Pi playability and load figures remain author observations.

Read SugarBeats' `docs/PASSDOWN.md` on 2026-11-16: accepted code `0851633`,
latest pushed docs `08e841d`, engine pin
`7085043a6a17a4909afe6ab46f48df4986bfdf3f`, 17/17 serial consumer checks and
four track-management checks reported passed. That passdown was modified in
its working tree; this review did not edit it. A0–C8 and one through nine tracks
are complete on both sides. No new nine-track cartridge/Pi acceptance is claimed.
Routine coordination remains dormant. The existing outward
`../SUGARBEATS_PASSDOWN.md` is a historical transfer note, not a second live
coordination authority; it was left untouched.

Reviewed graphics bindings/backend, asset reader and Lua library/budget setup,
music binding/shared ownership paths, timing, and smoke-runner coverage.
This is a focused review, not a whole-engine audit or hostile-code certification.

## Follow-up status

After review commit `e005b5d`, R1/R2 repairs were authorized and implemented in
a separate checkpoint. See [repair validation](../../tests/graphics-bounds.md)
for current results and pending acceptance. The findings below retain the
original review evidence; R3 runner changes remain unimplemented.

## Findings, in recommended repair order

### R1 — High: text coordinates can overflow signed C arithmetic

**Confirmed by UBSan.** `src/sb_graphics_sdl.c`, `draw_char()` and
`sb_graphics_text()` perform `x + col * scale`, `y + row * scale`,
`x += 8 * scale` and `y += 10 * scale` in `int`. The Lua binding accepts
coordinates/scale that reach these operations without safe arithmetic bounds.

A probe calling the real `l_sugarbee_print` with
`text('A', 2147483647, 0, 255, 255, 255, 1)` on SDL dummy produced:

```text
src/sb_graphics_sdl.c:134:37: runtime error: signed integer overflow:
2147483647 + 2 cannot be represented in type 'int'
```

That coordinate fits a signed 32-bit integer; fixing Lua narrowing alone will
not fix this. The native C interface also needs safe arithmetic. Undefined
behavior could produce incorrect rendering or a process failure; no connection
to the previously reported Pi OS lockups has been established.

Proposed repair: validate binding ranges, use checked/wider intermediate text
arithmetic, and safely skip fully offscreen glyph work. Preserve ordinary
negative/offscreen positions and existing text spacing. Add tests near both
integer extremes, large scale, newline advancement and ordinary output. Do not
choose an arbitrary small world-coordinate limit without discussing the contract.

### R2 — Medium: drawing bindings silently narrow Lua integers

**Confirmed on this desktop.** `src/main.c`, `l_sugarbee_rect`,
`l_sugarbee_print` and `l_sugarbee_sprite` cast Lua integers directly to `int`.
By contrast, `sprite_int_arg` checks `INT_MIN..INT_MAX` for region draws.
Out-of-range signed conversion is implementation-defined in C99.

A probe calling the real rectangle binding with
`rect(4294967296, 0, 1, 1, 255, 255, 255, true)` returned success and rendered
a white pixel at `(0,0)` instead of rejecting the unsupported coordinate.
A bad cartridge coordinate can silently draw in the wrong place. The input
player bindings use the same narrowing pattern and should be checked in the
bounded repair: their documented out-of-range result is false.

Proposed repair: consistently validate before narrowing, preserving documented
integer-valued numeric inputs and fractional-coordinate rejection. Test values
just outside the native range and ordinary negative coordinates. Color-channel
conversion is already documented; do not silently change its policy in this fix.

### R3 — Medium: the cartridge smoke runner has significant coverage gaps

**Confirmed by source review.** `tests/cartridge_smoke.lua` has no music,
WAV or save/load APIs; synth stubs always succeed. Valid cartridges using those
APIs cannot run unmodified through it. The host's Lua library restrictions,
execution budgets, native integer ranges and full archive/PNG checks are also
not modeled. Ordinary interpreter libraries can let unsupported game code pass.

The build guide previously promised matching argument validation and detection
of any console Lua error. This checkpoint corrects that claim. Runner code is
unchanged. Propose a separate coverage checkpoint: minimal documented stateful
stubs for the needed APIs, explicit unsupported behavior, and real-host contract
checks. Success-only audio stubs must not be used to validate song transitions,
voice ownership or refusal handling.

### R4 — Low: examples and current-status documentation had drifted

**Corrected here.** Input motion now floors only at the drawing boundary;
graphics docs explicitly describe partial-alpha blending. Asset docs distinguish
PNG, WAV and music loaders from missing generic file access. Music docs explain
stop/same-song bookkeeping and declared-track reservations. Timing docs qualify
updates-per-draw observations. README/roadmap now identify the accepted pitch
runtime instead of implying that the engine was unchanged since the music merge.
The runtime evidence identifies the committed acceptance record.

## Boundaries checked and remaining uncertainty

- The asset reader validates names, entry/archive sizes, duplicates of the requested
  member and CRC, and frees temporary buffers on failure. PNG loading frees encoded
  and decoded buffers before raising Lua errors. These inspected paths revealed no
  additional confirmed defect. Future modules/data should reuse bounded cartridge
  access, not enable general host filesystem loading or native module search.
- Graphics texture uploads enable alpha blending and nearest sampling; sprite
  regions validate source bounds by subtraction. Texture handles use generations
  and retire slots rather than wrap. No current tint/clip state exists to leak.
  Future state must reset across frames, host overlays, failures and relaunches.
- Music validates dense distinct mappings, reserves every declared track, refuses
  busy starts, and retains ownership through finishing. Stop/same-song friction
  describes stale cartridge bookkeeping, not a reproduced engine defect. A tested
  game-side transition example remains worthwhile before adding a queue API.
- `tests/test_lua_synth.c` already exercises a second waveform change during a
  transition, expects `audio_rejected`, and verifies no mutation. That test passed
  during this review. Report entry 16 is therefore verified at the engine contract
  level; incidence in the five games remains unknown.
- The 1,000,000-instruction callback limit is intentional; count hooks do not
  interrupt work inside C calls. Raising the limit or moving loops to native
  batches is not a safety fix. Any new batch/text/data operation needs bounded
  work. Aggregate Lua/texture memory and native-call time are not comprehensively
  budgeted today. No instruction-budget or memory-policy change is authorized.

## Disposition of all 33 friction entries

“Candidate” means discussion backlog, not accepted implementation. Existing
accepted future work retains that status without a schedule.

| # | Current disposition / destination |
| --- | --- |
| 1 | Camera/draw offset candidate; define world/UI separation and state reset. |
| 2 | Tile/batch candidate; reported 190 draws do not establish a bottleneck. Bound native work before batching. |
| 3 | Rect/text alpha candidate; related to accepted future sprite tint, but a separate API decision. |
| 4 | Logical-resolution work already planned; do not bundle it with clip/tint. |
| 5 | Cartridge-local Lua modules candidate; bounded loader/cache/error/lifetime contract required. |
| 6 | Packaging workflow candidate; use a cartridge-only input directory today. |
| 7 | Stable UUID/manifest workflow candidate; preserve save identity on repack. |
| 8 | Save deletion candidate; empty save remains an application convention. |
| 9 | Documentation corrected here; integers already required. R1/R2 are separate native robustness findings. |
| 10 | Text measurement/wrapping candidate; preserve existing font behavior. |
| 11 | Closed as not an issue; Start remains host-reserved. |
| 12 | Range/track restrictions resolved in current SugarBeats; noise and envelope editing remain composer work. No engine extension presumed. |
| 13 | Song-change ergonomics candidate; stop/wait/play contract remains intentional. |
| 14 | Export/package workflow candidate, not a runtime format defect. |
| 15 | Documented pitched/noise call distinction; keep contract, consider a game helper. |
| 16 | Existing native/Lua refusal test passes; occurrence in these games still unverified. |
| 17 | Dimension queries/text metrics candidate; C dimension queries already exist, Lua queries do not. |
| 18 | Closed as corrected in original report: runner exists. See 19/R3. |
| 19 | Confirmed runner gap, proposed tooling follow-up R3. |
| 20 | Clip rectangle is leading new graphics candidate; define coordinate space, reset and host-overlay behavior. |
| 21 | Rotation candidate; decide pivot, angle convention and clipping separately. |
| 22 | Integer source crop is current contract; remainder offset workaround valid. Link to resolution/camera discussion. |
| 23 | Reported cartridge bookkeeping fault; no engine defect reproduced. Test same-song restart during fade in future example. |
| 24 | Diagnostics candidate; updates/draw is not an exact FPS measurement. |
| 25 | Generic text/data access candidate; claim that only PNG loads is obsolete (WAV/music exist). |
| 26 | Sprite tint already accepted future work; ordinary multiplicative tint is not arbitrary palette replacement. |
| 27 | Resolved documentation ambiguity: partial sprite alpha blends. |
| 28 | Same clip-rectangle candidate as 20; source cropping is not viewport clipping. |
| 29 | Expected Lua budget termination; diagnostics/bounded batching discussion, not a renderer crash or automatic budget increase. |
| 30 | Lua active-local limit, not a universal 2,200-line limit; lexical scopes help, modules candidate ties to 5. |
| 31 | Native line primitive candidate; reusable 2D feature independent of a 3D engine. |
| 32 | Filled-triangle candidate; establish rasterization/clipping/cost separately from lines or general polygons. |
| 33 | Lua live-pitch API candidate; native bank/shared pitch setters already exist. Retrigger workaround reported acceptable. |

## Performance interpretation and next checkpoint

The five games' reported smooth Pi play is useful evidence for those workloads.
Small blended-sprite and small opaque-rectangle counts are not universal renderer
limits. Updates/draw cannot establish exact 60 FPS: the host clamps elapsed time,
limits catch-up and can drop backlog. Do not convert the report into a promised
5,000–7,500 draw-call budget. The earlier nine-voice audio benchmark also measured
an isolated workload, not these games or live SDL deadlines.

The report's broad 8 GB memory claim is not established: neither aggregate memory
use nor smaller Pi models was measured. Large blended surfaces, sustained thermal
load, WAV-heavy mixing and analog/multiplayer behavior were not covered by these
experiments. Analog input is not currently implemented. None of these omissions
invalidates the reported playability; they limit what can be concluded from it.

Recommend discussing R1/R2 as the next bounded repair checkpoint, then R3 and a
tested song-change example. Clip rectangles remain the first new graphics
candidate, followed by separately scoped camera, tint/alpha and line work.
Modules/data and a small host timing diagnostic need design discussions. Begin
any overlay with well-defined timings/counters; thermal/power readings, persisted
menu settings and a cartridge statistics API are separate choices. Keep the five
games as future visual acceptance examples when their sources are available.

## Verification of this review

- No engine, cartridge, runner or permanent test code changed.
- 22 existing targeted serial CTests passed across graphics/font/textures, assets,
  timing, Lua libraries/synth/music, music ownership/stop and eight sample games
  (includes the music fixture setup). No full-suite rerun or new Pi claim.
- Temporary probes compiled the actual host bindings and graphics backend, with
  `-fsanitize=undefined -fno-sanitize-recover=undefined`, using SDL dummy video.
  R2 produced success plus a white pixel at zero; R1 terminated with the overflow
  diagnostic above. These deliberately failing probes are review evidence, not
  passing regression tests. Files were kept in `/tmp/sugarbee-review-20261116`;
  they are disposable and are not required by the build.
- To reproduce R1 without the host harness, compile a small C program against
  `src/sb_graphics_sdl.c` and `src/font8x8.c` with the same sanitizer flags and
  SDL2. Initialize video, create a 64×48 graphics context, then call
  `sb_graphics_text(g, "A", INT_MAX, 0, 255, 255, 255, 1)` under
  `SDL_VIDEODRIVER=dummy`. A future repair must turn this into a safe result.
- Markdown links and whitespace were checked after editing. Source report content
  was retained verbatim apart from final-newline normalization.

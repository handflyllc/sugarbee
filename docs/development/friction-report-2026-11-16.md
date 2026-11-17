# SugarBee friction report

Where the SugarBee engine helped and where it got in the way, found by building five small games for it on 15 and 16 November 2026. Written to be handed over as input to future engine work. Nothing here changes the engine.

**The games**, all under `friction-testing/`, each with its own `DESIGN.md` and `README.md`:

| Label | Folder | Game | What it leaned on |
| --- | --- | --- | --- |
| `RPG` | `unnamedrpg/` | The Golden Comb, a top-down RPG | Tile maps, dialog, saving, music |
| `194Bee` | `194Bee/` | Vertical shooter | Scrolling backdrop, many sprites, a boss |
| `BeeForce` | `beeforce/` | Horizontal shooter | Parallax, tile terrain, translucent sprites |
| `BeeBot` | `beebot/` | Run-jump-and-shoot platformer | Physics, particles, three tile layers, load ladder |
| `BeeFlight` | `beeflight/` | Flight simulation | A 3D view built from rectangles, generated world |

All five were played on the desktop and on a Raspberry Pi 4 (8 GB), the baseline console.

## Summary

- **The engine held up.** Five games in five genres were built, and all five played on the Pi 4 without stutter. There are 33 entries below and none is an engine bug: 18 are missing capabilities, 12 are things that work but are clumsy, 2 are documentation, and 1 is an unconfirmed suspicion.
- **The Pi 4 has far more headroom than these games used.** It carried about 5,000 blended 32 px sprites, or about 7,300 small solid rectangles, a frame before missing frames. The heaviest game peaks at about 1,150 draw calls.
- **On a fast machine the Lua instruction limit is the ceiling, not the renderer** (entry 29). Passing it is a Guru Meditation with no warning, not a slowdown.
- **The drawing API is the main source of friction.** There is no camera offset, clip rectangle, rotation, tint, line or filled polygon. Each was worked around, at a cost in Lua code and draw calls. Bee Flight shows how far rectangles alone can be pushed, and what a line call would save.
- **One file is a real limit.** A single `main.lua` with no data files reached Lua's 200-local limit at about 2,200 lines (entries 5, 25, 30).
- **Audio worked well.** The synth effects and SugarBeats scores were liked, and a continuous engine drone was possible. The friction is in changing songs and in SugarBeats' limits being narrower than the engine's.
- **Every game after the first two rebuilt the same load readout**, which is the case for the statistics overlay under Engine ideas.

## Suggested priorities

A ranking by how often each gap came up and how much its workaround cost. It is one view, from the cartridge side only; it takes no account of how hard each change is in the engine. Entry numbers refer to the log.

**First: these shaped what could be built**

| Change | Entries | Why |
| --- | --- | --- |
| A way to see or survive the Lua instruction limit: instructions used so far, or a batch draw call | 29, 2 | The only finding that crashed a working game |
| More than one Lua file, and loadable data files | 5, 25, 30 | Maps as string literals; the 200-local limit |
| Clip rectangle | 20, 28 | Needed by three of the five games; no clean workaround |
| Camera or draw offset | 1 | Every world-space draw in every scrolling game subtracts it by hand |
| Sprite rotation | 21 | Rules out anything that must point where it is going |
| Tint, and alpha for rectangles and text | 3, 26 | Fades, hit-flash, colour variants, translucent panels |

**Second: these cost time but not features**

| Change | Entries | Why |
| --- | --- | --- |
| Line and filled-triangle calls | 31, 32 | One draw call where Bee Flight needs from 9 to 136 |
| Cartridge-chosen logical resolution, or a default scale | 4, 22 | Removes the 11-argument call for every sprite, and stepped scrolling |
| Screen size, text measuring and text wrapping | 10, 17 | 1280, 720 and 8 are hard-coded in every game |
| One-call song change, and a way to ask which song is playing | 13, 23 | The same pending-song code in every game with music, with a latent fault |
| A clock or load figure for cartridges | 24 | See the statistics overlay |
| Pitch change on a sounding note | 33 | The workaround is adequate |
| Delete a save | 8 | |

**Third: tooling and documentation**

| Change | Entries | Why |
| --- | --- | --- |
| Add music and save stubs to `tests/cartridge_smoke.lua` | 19 | A cartridge with music or saves fails in the runner as shipped |
| Say that drawing calls need whole numbers, and fix the float example | 9 | A fractional coordinate is a Guru Meditation |
| Say that partial alpha in sprites is blended | 27 | Confirmed by test; undocumented |
| Packaging: keep non-cartridge files out, remember the UUID, link SugarBeats exports | 6, 7, 14 | Manual steps on every repack |
| SugarBeats' authoring limits against the engine's | 12 | No percussion or low bass in a score that stays editable |
| `note_on` differs for noise voices | 15 | Minor |
| Confirm whether `audio_rejected` occurs in practice | 16 | Still unverified |

## What worked well

- **Performance on the Pi 4.** No game needed optimising to run smoothly.
- **Sprites with partial alpha are blended** (entry 27), which stands in for several missing features.
- **`sprite_region` cropping, scaling and flipping** carried every game's art.
- **The synth.** Nine voices were enough for music, effects and a continuous engine drone together.
- **SugarBeats to cartridge.** Scores written as `.sbp` projects exported and played.
- **The headless smoke runner** caught argument errors before a first run, once given the missing stubs.
- **Strict argument checking.** Bad calls fail loudly and early. The cost is entry 9.

## Not yet tested

| Gap | Why it matters |
| --- | --- |
| Fill rate: a few very large blended sprites | Every load test used many small things. The results suggest pixels drawn matter as well as call count |
| Heat over time | Every run was minutes long. A 30-minute soak would show whether the Pi throttles |
| WAV playback (`sugarbee.audio`) | No game used it |
| Memory | Cannot be judged on the 8 GB model |
| Entry 16 | The `audio_rejected` refusal was designed around, never provoked |
| Analog or multi-player input | No game needed either |
| A racing game | Considered and set aside: it would test rotation and steering but add nothing on load |

## The log

**Kinds:** `gap` = missing capability, `papercut` = works but clumsy, `doc` = documentation unclear or wrong, `bug` = engine misbehaves, `verify` = suspected, not yet confirmed by running.

**Status:** `open`, `confirmed`, `confirmed from source`, `not an issue`.

| # | Project | Date | Phase | Kind | Area | Friction | Workaround used here | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | RPG | 2026-11-15 | Design | gap | Graphics | No camera or draw offset. Every world-space draw call must subtract the camera position by hand. | One `draw_cell()` helper applies the offset. | open |
| 2 | RPG | 2026-11-15 | Design | gap | Graphics | No tilemap or batch draw. A frame of this game makes up to 190 separate draw calls from Lua (counted against a stub), 170 of them tiles. | Loop over visible tiles only. Measured since: the Pi 4 carries about 5,000 to 7,500 draw calls a frame (see Measurements), so this game was never close. | open |
| 3 | RPG | 2026-11-15 | Design | gap | Graphics | Rects and text are opaque; there is no alpha or tint. No fades, no translucent dialog box, no hit-flash by tinting, no dungeon darkness. | Black frame for map changes; blink sprites instead of tinting; solid dialog box. | open |
| 4 | RPG | 2026-11-15 | Design | gap | Display | Logical size is fixed at 1280×720. 16 px art must be scaled 5× on every draw call, and `sprite()` cannot scale, so everything goes through the 11-argument `sprite_region()`. | Wrapper function. A cartridge-selected logical resolution (already on the roadmap) would remove this. | open |
| 5 | RPG | 2026-11-15 | Design | gap | Lua | Single `main.lua`; no `require`. Map data, tuning tables and logic share one file of an expected 1,500+ lines. | Strict section order in the file. | open |
| 6 | RPG | 2026-11-15 | Design | papercut | Packaging | `sbee-pack` zips the entire input directory, so docs and tools cannot sit beside `main.lua`. | Cartridge files live in `cart/`. | open |
| 7 | RPG | 2026-11-15 | Design | papercut | Packaging | The game UUID must be remembered and passed by hand on every repack or the save is orphaned. | UUID recorded in `README.md`. Planned manifest would fix it. | open |
| 8 | RPG | 2026-11-15 | Design | gap | Saves | No way to delete a save. | Overwrite with an empty string and treat that as "no save". | open |
| 9 | RPG | 2026-11-15 | Build | doc | Graphics | Every drawing binding uses `luaL_checkinteger` (`src/main.c`), so a coordinate such as `100.5` is a Lua error and a Guru. The `rect` example in `docs/api/input.md` passes a float and only works because `180 * dt` happens to be exactly 3.0. | Floor every coordinate in one `spr()` helper. Either accept floats in the bindings or fix the example. | confirmed from source |
| 10 | RPG | 2026-11-15 | Design | gap | Text | One 8×8 font, integer scale only, no wrapping or measuring helper. | Dialog pages are split by hand. | open |
| 11 | RPG | 2026-11-15 | Design | papercut | Input | Start is reserved by the host, leaving Select as the only "menu" button. Not a problem for this game. | None needed. | not an issue |
| 12 | RPG | 2026-11-15 | Music | gap | SugarBeats | Authoring is limited to three tracks, triangle/saw/pulse, MIDI 45–81 and one fixed envelope, although the engine accepts nine tracks, noise, per-track envelopes and filters, and 27.5–4186 Hz. A score that must stay editable in SugarBeats cannot have percussion or a low bass. | Scores written inside the SugarBeats limits. | open |
| 13 | RPG | 2026-11-15 | Music | papercut | Music API | Changing songs is a three-step dance: `stop()`, poll `state()` until `idle`, then `play()`. Calling `play()` straight away returns `music_busy`. | A `pending` variable checked every `update()`. | open |
| 14 | RPG | 2026-11-15 | Music | papercut | Packaging | The exported `.json` must be copied into `cart/assets/` by hand after each SugarBeats edit; nothing links a project to a cartridge. | Export path written down in `README.md`. | open |
| 15 | RPG | 2026-11-15 | Build | papercut | Synth | `note_on` takes a frequency for pitched voices and must not be given one for a noise voice; the wrong form is a Lua error. Effect code has to remember each voice's waveform. | Effects table records the waveform; one branch in `sfx_advance`. | open |
| 16 | RPG | 2026-11-15 | Build | verify | Synth | The docs say a waveform change can be refused with `audio_rejected` while an earlier change is still in transition. A shared pool of effect voices would hit this. | Each waveform has dedicated voices, set once in `init()`. Not tested whether the refusal really occurs in practice. | open |
| 17 | RPG | 2026-11-15 | Build | papercut | Graphics | No way to ask for the screen size or measure text. 1280, 720 and the 8-pixel glyph width are hard-coded for centring. | Constants at the top of `main.lua`. | open |
| 18 | RPG | 2026-11-15 | Build | gap | Tooling | Corrected 2026-11-16: the engine repo does have a headless runner, `tests/cartridge_smoke.lua` (system Lua 5.4, stub API, random input). It was missed during the RPG build, which used a hand-written stub instead. Neither can catch real engine behaviour. See entry 19. | Stub-based tests on the author's side; first real run is manual. | not an issue |
| 19 | 194Bee | 2026-11-16 | Design | gap | Tooling | `tests/cartridge_smoke.lua` stubs graphics, input and `sugarbee.synth`, but has no `sugarbee.music`, `sugarbee.save` or `sugarbee.load_save`. A cartridge that uses music or saves fails in the runner on its first such call. | Test against a local copy of the runner with those three stubs added. | confirmed from source |
| 20 | 194Bee | 2026-11-16 | Build | gap | Graphics | No clip rectangle. A playfield narrower than the screen cannot stop sprites drawing over its edges. | Side panels are drawn last, as solid rects, and cover the overflow. Bee Flight had to cut every line and fill to its view in Lua for the same reason. | open |
| 21 | 194Bee | 2026-11-16 | Build | gap | Graphics | No sprite rotation. Enemies on curved paths and angled shots cannot point where they are going. | Enemies face down and are flipped vertically when climbing; angled shots stay upright. | open |
| 22 | 194Bee | 2026-11-16 | Build | papercut | Graphics | `sprite_region` source rectangles are whole image pixels, so a scaled backdrop scrolls in steps of the scale factor (3 px here) unless the game compensates. | Draw the slice one source row taller than the screen and shift it by the 0-2 px remainder. | open |
| 23 | 194Bee | 2026-11-16 | Build | papercut | Music API | `music.state()` says whether something is playing but not which song, so the cartridge keeps its own record, and that record can go stale: stop a song, ask for the same song again before the fade ends, and nothing plays. The RPG's audio section has the same latent fault (not fixed there). | Clear the cartridge's "current song" as soon as `stop()` is called. Found by a scripted test, not by ear. | open |
| 24 | BeeForce | 2026-11-16 | Design | gap | Timing | A cartridge cannot read a clock or the frame rate, so it cannot tell how loaded the host is. | Count `update()` calls per `draw()` call; more than one means draws are being missed. Shown on Select. Bee Bot and Bee Flight carry the same readout. | open |
| 25 | BeeForce | 2026-11-16 | Design | gap | Assets | Only PNGs can be loaded from a cartridge. Level data cannot be a separate text file, so a 264x15 map is 15 very long string literals in `main.lua`. | Map kept in `main.lua`; a script prints the first draft for pasting. | open |
| 26 | BeeForce | 2026-11-16 | Build | gap | Graphics | No sprite tint or palette swap. A red "carrier" version of an enemy needs its own cells in the sheet (8 extra here), and hit-flash has to be done by blinking. Related to entry 3. | Recoloured copies generated by the art script. | open |
| 27 | BeeForce | 2026-11-16 | Build | doc | Graphics | The graphics doc promises transparent or opaque sprite pixels but does not say whether partial alpha is blended. | Title screen draws an "ALPHA TEST" sprite with half-transparent pixels over stripes. Result, 2026-11-16: the stripes show through, so sprites with partial alpha are blended. Only the doc needs to say so. This also gives a workaround for entry 3: a translucent sprite stretched with `sprite_region` can stand in for a translucent rect. | confirmed |
| 28 | BeeForce | 2026-11-16 | Build | papercut | Graphics | Changing a repeating backdrop partway across the screen needs a clip rectangle (entry 20). Cropping the source instead only lands on multiples of the scale factor, so the join can sit up to 2 px off. | Draw the old zone in full, then the new zone cropped from the boundary. | open |
| 29 | BeeBot | 2026-11-16 | Build | papercut | Lua | On a fast machine the 1,000,000-instruction limit per callback, not the renderer, sets how much a cartridge can draw. Measured cost per draw call: 28 Lua instructions in the tightest possible loop, about 33 in ordinary game code, about 70 through two layers of helper function. So a cartridge tops out between about 14,000 and 35,000 draw calls a frame however fast the host is, and passing the limit is a Guru Meditation, not a slowdown. The first Bee Bot build did exactly that on the desktop. | The load ladder estimates the frame's cost and cuts itself short. A batch call (draw many sprites from one table) would lift the ceiling; a way to read instructions used so far would let a cartridge protect itself exactly. | confirmed |
| 30 | BeeBot | 2026-11-16 | Build | papercut | Lua | Lua allows 200 local variables at the top level of one file. With everything in a single `main.lua` (entry 5), a 2,200-line game reaches it: Bee Bot's last stage failed to load with "too many local variables". | Private variables moved inside `do ... end` blocks. Splitting a cartridge across files would remove the problem. | confirmed |
| 31 | BeeFlight | 2026-11-16 | Build | gap | Graphics | No line drawing. A wireframe 3D view needs lines at any angle, and the only way to make one is a chain of rectangles: one per step across the line's short direction. Measured at about 9 draw calls a line for small distant shapes, far more for long steep ones. On the Pi 4 the ceiling was about 1,700 lines a frame. | Line rasteriser in Lua, with near-plane and view clipping done by hand. A `sugarbee.line(x0, y0, x1, y1, r, g, b)` call would make each line one draw call. | open |
| 32 | BeeFlight | 2026-11-16 | Build | gap | Graphics | No filled triangle or polygon. A tilted horizon, a lake or a runway in perspective has to be filled one rectangle per row of blocks: up to 136 draw calls for one shape. | Scanline fill in Lua at 4 px blocks. A filled-triangle call would cover every case here. | open |
| 33 | BeeFlight | 2026-11-16 | Build | gap | Synth | No way to change the pitch of a sounding note. An engine drone that follows the throttle has to be started again at each new pitch. The documentation says a retrigger carries on from the current level and keeps the oscillator running, so with full sustain it passes as a glide. Heard 2026-11-16: the drone sounds great. | `note_on` again whenever the note has moved 1 Hz. A pitch field in `configure`, or a `set_pitch(voice, hz)` call, would do it properly, but the workaround is adequate. | confirmed |

## Measurements

Updates per draw is the yardstick: the engine runs extra updates when drawing falls behind, so 1.00 is a steady 60 frames a second. A load ladder adds a fixed batch of draws per step until that figure leaves 1.00.

| Date | Machine | What | Result | How |
| --- | --- | --- | --- | --- |
| 2026-11-16 | Pi 4 | RPG | Ran well | About 190 draw calls a frame |
| 2026-11-16 | Pi 4 | 194Bee | Ran as on the desktop, no issues | Not measured; the game has no load readout |
| 2026-11-16 | Pi 4 | Bee Force | Updates per draw steady at 1.00 | About 400 draw calls a frame at peak |
| 2026-11-16 | Pi 4 | Draw-call ceiling at 60 frames a second | About 5,000 a frame | Bee Bot stage 1 load ladder, step 19: 32 px translucent sprites added 250 at a time over the normal scene. Beyond this the Pi misses frames |
| 2026-11-16 | Desktop | Draw-call ceiling | Not found | Guru Meditation at roughly 11,000 to 13,000 draw calls, from the cartridge's Lua instruction limit (entry 29), before the frame rate moved. Not a limit of the machine |
| 2026-11-16 | Pi 4 | Bee Bot, finished game | No stutter | About 350 to 770 draw calls a frame outdoors, 900 to 1,150 indoors, with wind particles, four parallax layers and a third tile layer |
| 2026-11-16 | Pi 4 | Draw-call ceiling with small opaque rectangles | 7,100 to 7,500 a frame, about 1,700 wireframe lines | Bee Flight stage 1 load ladder, step 40 to 41, low over the city. Rectangles of 4 px blocks, mostly a few blocks long. Higher than the 5,000 found with 32 px translucent sprites, so blending or sprite size costs the Pi more than the call itself |
| 2026-11-16 | Pi 4 | Bee Flight, finished game, draw-call ceiling | About 6,500 to 7,800 a frame, about 1,500 wireframe lines | Load ladder, about step 37, in the busiest part of the scene. A little under stage 1 because the instruments and map are drawn too. The game itself uses 240 to 860 draw calls a frame |

What each game draws in ordinary play, for scale:

| Game | Draw calls a frame |
| --- | --- |
| RPG | up to 190 |
| Bee Force | about 400 at peak |
| Bee Bot | 350 to 770 outdoors, 900 to 1,150 indoors |
| Bee Flight | 240 to 860 |

Lua cost per draw call (entry 29): 28 instructions in the tightest loop, about 33 in ordinary game code, about 70 through two layers of helper. Bee Flight's heaviest ladder frame, 8,650 draw calls and 2,400 lines, used about 650,000 of the 1,000,000 allowed, measured off the engine.

## Engine ideas

Not friction entries: features suggested by the friction above, kept here for a later discussion.

### Statistics overlay (2026-11-16)

Jason's idea: a HUD overlay in the spirit of Minecraft's debug screen, drawn by the engine over any cartridge and switched on from the start menu (Start, Options, Statistics), alongside the planned system updates feature. Candidates to show: updates per draw under a clearer name such as "Load", CPU temperature, CPU speed.

Evidence from these projects that it is wanted:

- Bee Force, Bee Bot and Bee Flight each carry their own updates-per-draw readout on Select. That is the same code written three times, and it uses up a button.
- A cartridge can only see part of the picture. It can count its own updates and draws, but not the frame time, the Lua instructions used against the limit (entry 29), or anything about the machine (entry 24: no clock).
- The desktop Guru Meditation at the top of the load ladder gave no warning. A live "Lua instructions used" figure would have shown it coming.

#### Agreed outline (2026-11-16)

Baseline console: Raspberry Pi 4, tested on the 8 GB model only.

Audience: something a player can turn on, and a basic troubleshooting aid. Since the console is always a Pi 4, "can this hardware run this game" is really two different faults, and the overlay's main job is telling them apart:

- **The game is too heavy.** Frames are late while the Pi is cool, at full clock speed and properly powered. The cartridge author's problem.
- **The Pi is unhealthy.** Frames are late and the Pi is hot, throttled or under-voltage. A power supply, case or cooling problem.

**Page 1, for the player** (a short strip that can stay on while playing)

| Shown as | Built from |
| --- | --- |
| Speed, as a percentage, coloured green, amber or red | Share of frames delivered on time over the last second or so |
| Temperature in °C, coloured | CPU temperature |
| Power warning, only when there is a problem | Under-voltage flag |
| "Slowed down by heat" warning, only when there is a problem | Throttled flag |

**Page 2, for troubleshooting**

| Group | Figures |
| --- | --- |
| Keeping up | Frame time in ms split into update and draw; worst frame in the last second; updates per draw ("Load"); late frames since start; draw calls per frame; Lua instructions used against the limit, for update and draw |
| Cartridge | Lua memory in use; textures loaded and their total pixel size; engine process memory |
| Pi health | CPU temperature; throttled and under-voltage flags, now and since boot; CPU clock speed |
| Identity | Engine version; cartridge title |

**Session summary** (running totals since the cartridge started): share of frames on time, worst frame, peak temperature. This answers "can my console run this game" after a few minutes of play, and is what someone reporting a problem would quote.

Left for later: a frame-time graph, synth voices and music state, audio underruns, CPU use per core, cartridge UUID and save size, uptime. Left out: network, disk, and anything needing a system call every frame.

Notes:

- Memory will never be short on the 8 GB model, which is why the memory figures matter: they are the only way to judge whether a 1 GB or 2 GB Pi 4 could run the same cartridges without owning one. CPU and graphics are the same across the Pi 4 range.
- The overlay should draw after the cartridge and sample the slow readings (temperature, flags, clock speed) about once a second.
- Lua instructions and draw calls stay off the player page; they only matter to whoever wrote the cartridge.
- A draw-call count alone does not predict where a game falls over. The Pi's ceiling was about 5,000 calls with blended 32 px sprites and about 7,300 with small solid rectangles. Frame time is the figure to trust; draw calls help explain it.

Still to settle:

- Reached only through Start, Options, Statistics, or also by a button combination. Leaning towards the menu only, with the choice remembered, since a combination could clash with a cartridge's controls (only Start is reserved).
- Where the throttled and under-voltage flags can be read on the minimal console OS without `vcgencmd`. Not verified.
- Whether the overlay's own drawing is counted in what it reports.
- The thresholds for green, amber and red.
- Whether to offer the same figures to cartridges (for example a `sugarbee.stats()` call). A separate decision: it widens the API.

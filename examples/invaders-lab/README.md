# Invaders Lab: learn by building a small game

This is a post-Milestone 3 game-development experiment, **not an engine milestone**.
It intentionally uses the existing SugarBee API without new engine features.

## Step 1: flight test

**Goal:** Draw one player ship and move it left and right without leaving the
1280x720 logical viewport. No ammunition, enemies, collisions, scores, or sounds
at this stage. The ship consists of two colored rectangles so no PNG is required.

From the repository root:

```sh
./build/sbee-pack -i examples/invaders-lab -o /tmp/invaders-lab.vcart -t "Invaders Lab"
./build/sugarbee /tmp/invaders-lab.vcart
```

Use Left/Right, A/D, or a mapped gamepad D-pad to move the ship. Verify that it
stops at both screen edges. Exit normally when finished.

## Reading the Lua

- `local` introduces a variable used by this script; its name describes its job.
- `init()` runs once when the cartridge starts.
- `update(dt)` runs every simulation tick. `dt` is the elapsed simulation time in
  seconds; multiplying by `ship_speed` makes motion time-based rather than
  dependent on the number of update calls.
- `sugarbee.btn("left")` checks whether the left input is currently held.
- `math.min` and `math.max` constrain the X coordinate to the screen bounds.
- `draw()` clears the frame, draws two rectangles for the ship, and draws help text.
- `math.floor` converts the horizontal position to an integer for pixel drawing.

The next step, **only after this works**, will be a single player projectile.
Future steps may add enemies, rectangle collisions in Lua, scoring, and win/lose
states. No new engine subsystems are planned for this experiment.

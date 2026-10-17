# Pong Lab

A small Lua learning exercise using SugarBee’s existing graphics, input, and timing APIs.

## Current behavior

- Move one paddle vertically using Up/Down or W/S.
- The ball starts in a randomly selected diagonal direction.
- It bounces off the top, bottom, and right screen edges.
- A paddle hit reverses horizontal velocity while preserving vertical velocity.
- A miss is counted when the ball’s left edge reaches the left screen boundary.
- After a miss, the ball returns to the center and travels right, with a randomized vertical direction.

The drawn court is cosmetic. Movement and wall collisions use the full 1280×720 viewport.

## Package and run

From the repository root, with the engine and packager already built:

```sh
./build/sbee-pack -i examples/pong-lab -o /tmp/pong-lab.vcart -t "Pong Lab"
./build/sugarbee /tmp/pong-lab.vcart
```

Repack after changing the Lua source.

## Manual checks

- Move the paddle to both vertical limits; it should remain inside the screen.
- Launch several times to observe different initial ball directions. Repeated directions are normal.
- Observe bounces at the top, bottom, and right boundaries.
- Intercept the ball; it should bounce right without changing vertical velocity.
- Miss the ball; the counter should increase once and the ball should restart from the center toward the right.
- Miss again; the counter should increase again.

This checkpoint has one paddle and a practice backstop. Two-player controls and competitive scoring are not implemented.
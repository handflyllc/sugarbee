# Pong Lab

A small Lua learning exercise using SugarBee’s existing graphics, input, and timing APIs.

## Current behavior

- Two players control independent paddles using their controllers’ D-pads.
- Player 1 can also use Up/Down or W/S on the keyboard.
- Controllers use input slots 0 and 1, assigned in discovery order.
- The ball starts in a randomly selected diagonal direction.
- It bounces off the top and bottom screen edges.
- Either paddle reverses horizontal velocity while preserving vertical velocity.
- When the ball reaches a left or right screen boundary, the opponent earns one point.
- After a point, the ball returns to center and travels toward the player who scored, with a randomized vertical direction.
- First to five points wins, and movement stops.
- After a win, B on either controller—or keyboard K—resets both scores and paddle positions and starts a new match.

The drawn court is cosmetic. Movement and scoring boundaries use the full 1280×720 viewport.

## Package and run

From the repository root, with the engine and packager already built:

```sh
./build/sbee-pack -i examples/pong-lab -o /tmp/pong-lab.vcart -t "Pong Lab"
./build/sugarbee /tmp/pong-lab.vcart
```

Repack after changing the Lua source.

## Manual checks

- Move both paddles independently and simultaneously.
- Move each paddle to both vertical limits; neither should leave the screen.
- Observe ball bounces at the top and bottom boundaries.
- Return the ball with either paddle; vertical velocity should remain unchanged.
- Miss on each side; only the opponent’s score should increase, once per miss.
- Confirm each new rally starts at center, traveling toward the player who scored.
- Reach five points with either player; confirm the correct winner appears and movement stops.
- Restart after a win using B on each controller in separate tests.
- Confirm restart clears scores, centers the paddles, and resumes play.
- Confirm B has no effect during an active match.

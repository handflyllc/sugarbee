# Pong Lab

A small Lua learning exercise using SugarBee’s existing graphics, input, timing, and synth APIs.

## Current behavior

- Two players control independent paddles using their controllers’ D-pads.
- Player 1 can also use Up/Down or W/S on the keyboard.
- Controllers use input slots 0 and 1, assigned in discovery order.
- The ball starts in a randomly selected diagonal direction.
- It bounces off the top and bottom screen edges.
- Either paddle reverses horizontal velocity while preserving vertical velocity.
- Wall reflections play a short 330 Hz triangle blip; paddle reflections play
  a higher 660 Hz blip. Separate voices permit simultaneous contacts.
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

Repack after changing the Lua source. Use `--console` before the cartridge path
when checking Exit to system.

## Audio checkpoint

Voice 1 is the wall sound; voice 2 is the paddle sound. Each uses a 3 ms attack,
25 ms gameplay gate (rounded up to the next update), and 60 ms release. The gate
is refreshed on a repeated hit; it releases even on the winner screen. The
existing host freezes audio and gameplay during pause and clears it on exit.
Shutdown also requests short stops for both voices. Audio unavailability does
not alter gameplay. There are no scoring, win, startup, or background sounds.

Tests cover both wall and paddle directions, separation after a bounce, misses,
simultaneous contacts, winner-screen release, and unavailable output. A real-host
cartridge test exercises play and relaunch with dummy/unavailable output. The
existing smoke runner supplies audio stubs; real binding validation belongs to
the native Lua tests. Development Debug build completed without compiler
warnings; all 83 automated tests passed. The user confirmed all desktop checks
passed on coastline, including the listening checklist. Pi validation is pending.

Listen for a lower wall blip and higher paddle blip, with no stuck tones. Check
pause/resume, exit while sounding, and silent relaunch before the next collision.
Then check Pac-Man and Exit to system status 0. Two-player controls and gameplay
remain unchanged; test both paddles when two controllers are available.

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

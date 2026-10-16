# Invaders Lab: learn by building a small game

Invaders Lab is a game-development experiment using SugarBee’s existing Lua API. It is not an engine milestone and requires no new engine features.

## Current gameplay

Move your ship and fire at a formation of five enemies. Only one player bullet can be active at a time.

The formation moves horizontally, reverses direction at the screen margins, and descends on each reversal. Its travel limits follow the outermost living enemies: destroying an outer enemy lets the survivors travel farther toward that edge. Destroying a middle enemy leaves a gap without changing the boundaries.

A bullet overlapping a living enemy destroys that enemy and deactivates the bullet. Subsequent shots pass through the empty slot.

Destroy all enemies to win. If the formation reaches the ship’s defensive line, you lose. Reaching that line takes priority over bullet collisions during the same update.

After either outcome, gameplay stops until you restart. Restart restores the ship, all five enemies, the original formation boundaries, and the initial game state.

## Build and run

From the repository root, with SugarBee and its packager already built:

```sh
./build/sbee-pack -i examples/invaders-lab -o /tmp/invaders-lab.vcart -t "Invaders Lab"
./build/sugarbee /tmp/invaders-lab.vcart
```

Repack after changing the Lua source; the host runs the packaged cartridge.

## Controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move left/right | Left/Right arrows or A/D | D-pad left/right |
| Fire | J | A |
| Restart after victory or defeat | K | B |

Firing responds to a new button press rather than continuous holding. Release and press again to fire another shot once the previous bullet is gone.

## Manual gameplay checks

Use separate runs or restart between checks as needed.

- Move to both screen edges and verify the ship remains inside the viewport.
- Fire and miss: the bullet should disappear after leaving the top of the screen.
- Hit an enemy: both the enemy and bullet should disappear.
- Fire through a destroyed enemy’s slot: the bullet should continue upward.
- Destroy only a middle enemy: formation travel limits should remain unchanged.
- Destroy an outer enemy on each side: surviving enemies should travel to the corresponding screen margins.
- Destroy all enemies: the victory message should appear and gameplay should stop.
- Allow the formation to reach the defensive line: the defeat message should appear and gameplay should stop.
- Press K after either outcome: all five enemies should return, movement should resume, and the original formation boundaries should be restored.

The same-update defeat priority can also be checked in the code: the `game_over` guard after formation movement returns before bullet collision processing.

## Reading the Lua

- `init()` runs once when the cartridge loads.
- `update(dt)` advances game logic at the fixed simulation rate. Movement uses speed multiplied by `dt`.
- `draw()` renders the current state. Fractional simulation positions are rounded down at the drawing boundary.
- `enemy_positions` holds fixed starting X coordinates; `formation_offset` supplies their shared horizontal displacement.
- `enemy_alive` tracks each enemy using matching table indices. Dead enemies retain their slots.
- Rectangle overlap detects bullet hits against living enemies.
- The first and last living slots determine the formation’s movement limits.
- `game_over` and `victory` stop normal updates while allowing restart.
- `reset_game()` restores the initial playable state.
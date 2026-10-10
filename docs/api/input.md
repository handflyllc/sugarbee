# SugarBee input API (experimental v0.1)

The controller subsystem is implemented through the internal language-neutral
C interface in [`include/sb_input.h`](../../include/sb_input.h). The host uses
`src/sb_input_sdl.c` for SDL2 device management and event polling. Lua receives
logical button states; **game code never uses SDL controller identifiers**.
This interface may change before a stable SugarBee SDK is released.

## Lua game API

| Function | Meaning |
| --- | --- |
| `sugarbee.btn(name [, player])` | `true` while the logical button is held. |
| `sugarbee.btnp(name [, player])` | `true` on the first poll after the logical button changes from released to held. |

`player` defaults to `0` and is zero-based (0–3). Invalid button names and
out-of-range players return `false`. Button names are lowercase and
case-sensitive: `up`, `down`, `left`, `right`, `a`, `b`, `x`, `y`, `start`,
`select`.

Example:

```lua
function init()
    x = 100
end
function update()
    if sugarbee.btn("left") then x = x - 3 end
    if sugarbee.btn("right") then x = x + 3 end
    if sugarbee.btnp("a") then print("A was pressed") end
end
function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.rect(x, 100, 32, 32, 240, 180, 50, true)
end
```

The `3`-pixels-per-update speed is temporary; SugarBee does **not** yet provide
a fixed-step update or `dt`, so movement is currently frame-rate-dependent.

## Button mapping

SDL's standardized GameController buttons map to SugarBee's logical buttons:
A/B/X/Y, D-pad directions, Start, and Back (exposed as `select`). Analog sticks,
triggers, and axes are **not supported** in this iteration.

For **development**, the keyboard is an additional player-0 controller:

| SugarBee input | Keyboard |
| --- | --- |
| Directions | Arrow keys or W/A/S/D |
| A / B | J / K |
| X / Y | U / I |
| Start | Enter |
| Select | Right Shift |

The keyboard and player-0 gamepad are combined: either can hold a logical
button. Each gamepad occupies the lowest available player slot, 0 through 3.
Disconnecting a gamepad releases that controller's held buttons and frees the
slot; a subsequently connected gamepad uses a free slot. No gamepad is required
to launch a cartridge. The keyboard fallback only applies to player 0.

## Engine lifecycle and event model

- The host initializes SDL's video and gamecontroller subsystems before
  constructing the input context.
- `sb_input_create()` discovers attached controllers and owns their handles.
- `sb_input_poll()` drains SDL's event queue once per host loop, processes
  input and device changes, and returns `false` for quit/window close.
- `sb_input_down()` and `sb_input_pressed()` read the resulting snapshots.
- `sb_input_destroy()` closes opened controllers before SDL shutdown.

**Important:** The current input backend owns the SDL event queue, which is
sufficient while the host needs only input, focus, hotplug and quit events.
A future platform event dispatcher may replace this when other subsystems
require SDL window or device events.

`btnp` is a transition between **polled snapshots**, not an event queue. A
press and release entirely between two polls may not register. Fast taps,
fixed-step update delivery, and precise input timing are future improvements.
Keyboard state is cleared on focus loss so keys do not remain stuck.

## Testing

`input_smoke` runs with `SDL_VIDEODRIVER=dummy`. It verifies logical keyboard
mapping, held/pressed semantics, player bounds, focus-loss cleanup, quit handling,
and optional virtual-controller hotplug on SDL builds that support it.

Also test manually with a real controller: connect before startup, move the
sample box, disconnect/reconnect the controller, and try keyboard movement.
The earlier gold-rectangle and no-cartridge Guru tests should still pass.

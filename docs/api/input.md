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
| `sugarbee.btnp(name [, player])` | `true` during the first simulation `update(dt)` after a released-to-held transition. |

`player` defaults to `0` and is zero-based (0–3). Invalid button names and
out-of-range players return `false`. Button names are lowercase and
case-sensitive: `up`, `down`, `left`, `right`, `a`, `b`, `x`, `y`, `start`,
`select`.

Example:

```lua
function init()
    x = 100
end
function update(dt)
    if sugarbee.btn("left") then x = x - 180 * dt end
    if sugarbee.btn("right") then x = x + 180 * dt end
    if sugarbee.btnp("a") then print("A was pressed") end
end
function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.rect(x, 100, 32, 32, 240, 180, 50, true)
end
```

Movement now uses a nominal fixed 60 Hz simulation timestep. `dt` is measured
in seconds and is currently always 1/60. See [timing](timing.md).

## Host pause menu

Start on any connected controller, or Enter on the keyboard, opens the
host-controlled pause menu. Resume is selected each time the menu opens.

- Up/Down selects Resume or Exit Game.
- A confirms the selected action.
- B or Start resumes play regardless of the selection.
- Keyboard equivalents are Up/Down or W/S, J to confirm, and K to resume.

Start is reserved for the host. Cartridge calls to `sugarbee.btn("start")`
and `sugarbee.btnp("start")` always return false.

While paused, the host stops calling Lua `update()` and `draw()`. It displays
the retained game image beneath the menu. Menu input is consumed by the host;
buttons still held when resuming remain hidden from the cartridge until
released.

Exit Game closes SugarBee through its normal shutdown path, including the
optional Lua `shutdown()` callback when eligible. Returning to a cartridge
launcher and powering off the operating system are not implemented.

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
  input and device changes, and returns `false` for quit/window close. It also
  buffers button-press edges until the next simulation update.
- The host calls `sb_input_begin_update()` and `sb_input_end_update()` around
  each fixed host step, consuming a press at most once. During pause these
  steps continue handling menu input, but do not invoke Lua `update(dt)`.
- `sb_input_down()` returns the most recently polled held state;
  `sb_input_pressed()` reports an edge during the active simulation update.
- `sb_input_destroy()` closes opened controllers before SDL shutdown.

**Important:** The current input backend owns the SDL event queue, which is
sufficient while the host needs only input, focus, hotplug and quit events.
A future platform event dispatcher may replace this when other subsystems
require SDL window or device events.

`btnp` edges are **buffered between SDL polls and fixed updates**, including
press-and-release events processed in a single poll. Multiple presses before
an update coalesce to one boolean edge; it is not a lossless event queue.
`btnp` should be called in `update(dt)`, not in `draw()` or `init()`.
See [timing](timing.md). Keyboard state is cleared on focus loss so keys do
not remain stuck.

## Testing

`input_smoke` runs with `SDL_VIDEODRIVER=dummy`. It verifies logical keyboard
mapping, held/pressed semantics, player bounds, focus-loss cleanup, quit handling,
and optional virtual-controller hotplug on SDL builds that support it.

Also test manually with a real controller: connect before startup, move the
sample box, disconnect/reconnect the controller, and try keyboard movement.
The earlier gold-rectangle and no-cartridge Guru tests should still pass.
Initial physical-controller reference: **Logitech Gamepad F310**, used by the
project developer in **Flight Mode**. Other controllers may work through SDL;
we are not undertaking a compatibility certification effort at this stage.

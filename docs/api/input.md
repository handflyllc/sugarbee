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
out-of-range players return `false`, including integers outside the native C
range (they cannot wrap into a valid player slot). Button names are lowercase and
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
    sugarbee.rect(math.floor(x), 100, 32, 32, 240, 180, 50, true)
end
```

Movement now uses a nominal fixed 60 Hz simulation timestep. `dt` is measured
in seconds and is currently always 1/60. Keep `x` fractional during updates;
`math.floor(x)` converts only the drawing coordinate to the required integer.
This remains valid when the movement speed changes. See [timing](timing.md).

## Explicit text entry

Opt-in text entry belongs to the active cartridge. Existing games keep their
keyboard-to-player-0 mapping, with Escape added as a host Start shortcut.

| Function | Contract |
| --- | --- |
| `sugarbee.text_begin()` | Enable keyboard text entry. No arguments or results; only in healthy `init()`/`update()` callbacks. Idempotent while already enabled. |
| `sugarbee.text_end()` | Disable text entry. Same callback/argument/result rules. Idempotent while disabled. |
| `sugarbee.text_events()` | Consume this update's event batch. No arguments; returns exactly `(events, error_or_nil)`. Only in a healthy `update()` callback. |

These calls raise Lua errors for arguments (including explicit nil), unsupported
callback phases, missing input context or unavailable/failed/paused cartridge
sessions. They cannot be used during top-level loading, `draw()` or `shutdown()`.
Successful end/begin does not create or change a game-owned text buffer.

`events` is a fresh dense array in platform event order. Each printable ASCII
character (32–126) is `{type='text', text='a'}` with exactly one byte of text.
Editing events have just a `type` field: `backspace`, `delete`, `left`, `right`,
`home`, `end`, or `submit`. Enter and keypad Enter generate `submit` on a fresh
press, never on a key repeat. Platform repeat is accepted for printable text and
editing keys; no engine-generated repeat timer is added. Shift/layout processing
comes from SDL text events, rather than translating keycodes to characters.
Escape is always reserved for the host and never delivered as text/edit input.
Tab, Up/Down, modifier keys and other unlisted keys have no editing event.
Selection shortcuts, clipboard access, composition and Unicode are unsupported.

A pending batch holds at most **128 events**, including characters and editing
actions. It accumulates across polls until the next fixed update. Exceeding the
limit rejects the entire pending batch: `{}, 'overflow'`. Non-ASCII/control text,
nonterminated platform text or nonempty composition events reject the entire
batch: `{}, 'unsupported_text'`. The first rejection wins; subsequent events,
including Enter, are discarded until that batch is handed to the next update.
This avoids submitting silently truncated or altered commands. No rejection
changes the game's buffer; the game must handle the status explicitly.

The first read in an update consumes the batch; later reads return `{}, nil`.
An unread batch expires at the end of that update. Catch-up updates never replay
it. Disabled text entry returns `{}, nil`. There is one input mode/batch for the
whole cartridge, not per-player or per-field. The game routes it to its focused
editor, and owns buffer limits, cursor, rendering, submission and history.

```lua
-- Within update(), while your editor is focused:
local events, status = sugarbee.text_events()
local submitted, err = editor:apply(events, status) -- example Lua helper
if submitted ~= nil then
    sugarbee.text_end()
    -- Interpret/store submitted text in your game.
end
```

While typing, keyboard gameplay mappings (WASD/arrows, J/K/U/I and Right Shift)
are suppressed. Physical gamepads remain available, and controller Start remains
host-reserved. Games choose whether movement or other gamepad actions continue.
Beginning/ending text entry discards pending text and keyboard edges, blocks held
keyboard keys until release and flushes queued platform text/composition events.
Physical controller edges are preserved. To avoid repeating an activation/menu
key into the editor, text/edit events are also ignored while any key held across
a mode transition remains held; release those keys before typing.

Pause suspends platform text input and temporarily restores keyboard menu
mappings. Resume restores the requested text mode. Both discard queued typing;
held menu keys must be released before typing resumes. The game-owned line and
cursor survive because Lua state survives pause. Focus loss clears keyboard
state and pending typing, stops platform text input and retains the requested
mode. Focus return restarts it; repeat events without a fresh key press are
suppressed. Focus loss does not automatically pause game simulation.

Exit, failed startup and Guru reset text mode and queues. A fresh cartridge
session starts with text entry off. Held typing keys cannot become movement or
host actions after end/reset until released. Escape works for all games, not
just text-enabled ones; it also acts as Start in console-mode idle dialogs.

See the [text entry demo and reusable single-line editor](../../examples/text-entry-demo/README.md).
The helper's 256-character cap is a game-side policy, distinct from the engine's
128-event batch limit. General keyboard polling, mouse input, arbitrary key
bindings and multiline editors are not added by this API.

## Host pause menu

Start on any connected controller, or Escape on the keyboard, opens the
host-controlled pause menu. Enter also opens it outside explicit text entry;
while typing, Enter submits to the game. Resume is selected each time the menu opens.

- Up/Down selects Resume or Exit Game.
- A confirms the selected action.
- B or Start resumes play regardless of the selection.
- Keyboard equivalents are Up/Down or W/S, J to confirm, and K to resume.
  Enter or Escape also resumes from the host menu.

Start is reserved for the host. Cartridge calls to `sugarbee.btn("start")`
and `sugarbee.btnp("start")` always return false.

While paused, the host stops calling Lua `update()` and `draw()`. It displays
the retained game image beneath the menu. Menu input is consumed by the host;
buttons still held when resuming remain hidden from the cartridge until
released.

Exit Game ends the cartridge session, invokes the optional Lua `shutdown()`
callback when eligible, and returns to an idle host screen. The window and
controllers remain available.

## Launcher and error recovery

With a direct cartridge path, A on any controller or keyboard J relaunches
that cartridge from the idle screen. With `--cartridges DIRECTORY`, Up/Down
(or arrows/W/S) selects a listed cartridge and A/J launches it. Selection
moves once per press, stops at either end, and scrolls through eight visible
rows. Exit Game returns to the list with the selection preserved.

Without `--console`, B/K closes the idle host or launcher and Start has no
effect there. An empty
list ignores navigation and confirmation. Without startup arguments, the
idle screen displays launch instructions. Buttons held during launch remain
hidden from the cartridge until released. B takes priority over rescan; rescan
takes priority over confirmation; confirmation takes priority over navigation
when received together. X on a controller, or U on the keyboard, rescans the
directory and selects the first entry without launching it.

Guru Meditation keeps the diagnostic visible until B/K returns to the list
or idle host. Recovery consumes the press, cleans up the failed session, and
skips the cartridge shutdown callback. Holding B/K does not also close the
host; release and press again to close it. Full diagnostics remain in the
console. See [launcher usage and limits](../development/launcher.md).

In directory mode, a startup scan automatically launches a sole eligible
cartridge if there are no scan errors or omitted eligible entries. Exiting,
recovering from Guru, or rescanning never triggers automatic launch.
With the explicit leading `--console` flag, idle B/K does nothing and
Start/Enter/Escape opens an Exit to system confirmation with Cancel selected.
Up/Down selects; A/J activates; B/K or Start/Enter/Escape cancels. While the panel is
open, launch, rescan, and list navigation are blocked. Confirming Exit to system
ends the host loop, runs normal cleanup, and returns to the launching terminal.
The OS continues running; operating system power-off is not implemented.
See the launcher guide for the full flow.

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
| Start | Enter (outside text entry), Escape |
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
- During Guru Meditation, a separate host input pass consumes presses for
  recovery without invoking cartridge callbacks.
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
The gold-rectangle, no-argument idle-host, and Guru recovery checks should
still pass. See the [launcher checklist](../../tests/launcher/README.md).
Initial physical-controller reference: **Logitech Gamepad F310**, used by the
project developer in **Flight Mode**. Other controllers may work through SDL;
we are not undertaking a compatibility certification effort at this stage.

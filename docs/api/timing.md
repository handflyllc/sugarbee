# SugarBee timing and lifecycle (experimental v0.1)

The game uses these Lua callbacks:

| Callback | When it runs |
| --- | --- |
| `init()` | Once per launch after the cartridge script loads successfully |
| `update(dt)` | Zero or more fixed simulation steps per host render iteration while healthy and unpaused |
| `draw()` | Once per host render iteration while healthy and unpaused |
| `shutdown()` | Optional; called once when an initialized cartridge session ends, unless Guru Meditation is active |

`dt` is **always 1/60 second** (`0.016666...`) at present, regardless of
actual renderer speed. Lua functions declared as `update()` continue to work:
Lua ignores extra arguments. An optional `shutdown()` callback runs before
the host closes the cartridge's Lua state and releases its textures. Input and
graphics services remain available during the callback. It receives no arguments,
and its return values are ignored.

The callback is skipped if cartridge initialization failed or Guru
Meditation is active. An absent or non-function `shutdown` value is
ignored. Errors raised by the callback are reported to the console;
native cleanup still continues.

Closing the window ends the cartridge session and closes SugarBee. Selecting
Exit Game instead ends the cartridge session and returns to an idle host.
The host closes Lua and releases cartridge textures while retaining its
window, renderer, and input services. Closing the idle host does not call the
cartridge shutdown callback again.

The idle host can relaunch the startup cartridge, or launch the selected
entry in directory mode. Each launch
creates a fresh Lua state, registers the SugarBee API, loads the script, and
calls `init()`. Lua globals from the previous session do not carry over.
The scheduler is reset after launch so loading time does not become catch-up
gameplay. A failed launch displays Guru Meditation.

B/K dismisses Guru Meditation and returns to the list or idle host. Failed
sessions are cleaned up while Guru is still active, so their shutdown callback
is skipped. Recovery input is consumed and timing is reset. A healthy cartridge
can then be launched in a fresh Lua state. Discovery reads the header and
`main.lua` but neither compiles nor executes Lua; eligibility is not a guarantee
that initialization or gameplay succeeds.

Callback execution has no time limit, so a callback that never returns can
prevent the host from processing menu input or completing cartridge cleanup.

```lua
local x = 100
function init() end
function update(dt)
    if sugarbee.btn("right") then x = x + 180 * dt end
    if sugarbee.btnp("a") then print("A pressed") end
end
function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.rect(x, 100, 32, 32, 240, 180, 50, true)
end
```

## Scheduler behavior

- The SDL host measures elapsed time with SDL performance counters.
- `sb_timing_advance()` is a **pure-C**, platform-neutral accumulator.
- One host render iteration can have zero, one, or several calls to `update`.
- Up to five simulation updates are performed per host iteration. When rendering
  stalls long enough to exceed that budget, excess accumulated time is dropped
  to avoid an unbounded catch-up spiral. This means gameplay can **slow down**
  under sustained overload; SugarBee does not guarantee real-time simulation.
- A single measured elapsed interval is capped at 250 milliseconds before it is
  added to the accumulator (e.g. after a debugger pause).
- Rendering currently takes place once per host iteration with SDL VSync
  requested. Rendering frequency and simulation frequency are not the same
  guarantee. Interpolation between simulation states is **not implemented**.

## Host pause behavior

While the host menu is open, neither Lua `update()` nor `draw()` is called.
The host continues polling input and advancing its scheduler, so paused time
does not accumulate into catch-up updates on resume.

The graphics backend retains the last game frame and copies it to the window
before drawing the host menu. Menu overlays do not modify the retained image.

The input guarantees below describe normal gameplay. Start is host-reserved,
menu presses are consumed, and buttons held across resume are suppressed until
released. See [input](input.md#host-pause-menu) for controls.

## Input at update boundaries

The host polls SDL events each render iteration and stores press transitions
until the next simulation update. `sb_input_begin_update()` exposes pending
edges to that update, and `sb_input_end_update()` clears them. This ensures:

1. When no `update` occurs during a rendered frame, a button press is retained.
2. When multiple updates occur in one frame, `sugarbee.btnp()` is true during
   **only the first** update after the press.
3. A quick press and release received during a single SDL poll still produces
   one `btnp` edge on the next update (although `btn` may already be false).

`btn()` reports the most recently polled held state, not a full history of input
events. Multiple presses between simulation updates are coalesced into one
boolean edge; lossless input/event queues are out of scope for v0.1. Query
`btnp()` in `update(dt)`, not in `draw()` or `init()`.

## Tests and compatibility

`timing_smoke` tests the accumulator without SDL or Lua, including zero/multiple
updates, invalid elapsed values, and bounded catch-up. `input_smoke` tests
buffered edges across zero and multiple simulation updates, short taps, and
existing gamepad/keyboard behavior. The original no-argument `update()` Lua
callback is compatible with `update(dt)` calls.

This timing API is not yet a stable public SugarBee ABI.

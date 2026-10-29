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
native cleanup still continues. Shutdown has its own fresh provisional allowance
of 1,000,000 Lua instructions. Exceeding it reports a console error without
activating Guru, even if the callback catches the limit error and returns.
The callback is not retried; native cleanup follows the existing exit path.

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

Top-level `main.lua` execution and `init()` each receive a fresh provisional
allowance of 10,000,000 Lua instructions. File reading and compilation are not
covered. Startup limit failures skip shutdown and use normal Guru recovery.

Each main-thread `update()` and `draw()` call has its own fresh provisional
allowance of 1,000,000 Lua instructions. Nested Lua functions share the calling
stage's allowance. Exceeding it shows an execution-limit Guru; B/K returns to the host
and the failed session skips shutdown. The hook is cleared after each protected call returns.
A caught limit error that returns to the host still fails the session.

These limits catch accidental Lua loops, not deliberate bypasses.
They do not guarantee escape from repeatedly caught errors. Cartridge-defined
`__gc` finalizers are excluded by the metatable policy below. Coroutine and debug libraries are not exposed to
cartridges. Native calls cannot be interrupted by this hook. There is no wall-clock
limit; uncovered execution can still prevent menu input or cleanup.
See [execution-limit checks](../../tests/execution-limit/README.md).

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

## Supported Lua libraries

Cartridges receive the base, table, string, math, and UTF-8 libraries, plus
SugarBee's API. The coroutine, debug, io, os, and package libraries are not
opened. `require`, `dofile`, and `loadfile` are unavailable. This intentionally
excludes direct filesystem access, OS commands, native-module loading, and
cartridge control of debug hooks. Attempts to use missing globals produce
ordinary Lua errors through the existing lifecycle error handling.

Base functions such as `print`, `pcall`, and `load` remain available. `load`
compiles supplied Lua code; it does not load a filesystem path. Compilation
and native operations remain outside the instruction budget. Memory exhaustion,
`__close` behavior, and repeatedly caught errors still require separate
consideration: this policy is not a hostile-code sandbox.

`setmetatable` preserves ordinary Lua behavior, but rejects any proposed
metatable with a raw, non-nil `__gc` field, including `false`. The error is
`Cartridge __gc finalizers are not supported`. Shared metatables, ordinary
metamethods, protected metatables, and removing a metatable remain supported.
Adding `__gc` to an already attached metatable does not register its existing
objects for finalization; attaching it again (to the same or another table)
is rejected. Removing that field allows attachment again. An inherited `__gc`
through the metatable's own `__index` is not a finalizer registration.

Automatic memory collection remains enabled. Use the eligible `shutdown()`
callback for cartridge-level cleanup; the host owns native resource cleanup.
This restriction prevents cartridge table finalizer registration through the
supported API. Future native APIs exposing userdata must review their own
finalizers and metatables. Lua's distinct `__close` mechanism is not restricted
by this checkpoint.

This is a compatibility restriction for cartridges using previously exposed
standard libraries. Existing bundled games do not depend on the removed APIs.
The standalone cartridge smoke runner uses the system Lua interpreter and is
not itself restricted; `lua_supported_libraries` tests the real host setup.

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

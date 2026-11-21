# Developer timing readout

Enable with a leading `--stats`; off by default in both Debug and Release builds:

```sh
./build/sugarbee --stats build/stats-demo.vcart
./build/sugarbee --stats --console --cartridges build
./build/sugarbee --help
```

Option order is `[--stats] [--console] [--save-dir DIRECTORY]`, followed by an
optional cartridge path or `--cartridges DIRECTORY`. Existing commands still work.
The host draws three abbreviated lines at the top left, independently of the
cartridge canvas, camera, clipping and tint. It can cover game content; it does
not modify the retained game image. This is an opt-in developer tool.

| Field | Meaning |
| --- | --- |
| FPS | Number of completed host presentation calls in the last second. Not a guarantee of physical display refresh. |
| Frame | Average / maximum interval between completed presentation calls, in milliseconds. Includes host work, waiting, and diagnostic overhead. |
| Upd | Average total wall time in game `update()` calls per presented frame, in milliseconds. Includes all catch-up updates and native calls made by Lua; frames with no update contribute zero. |
| Draw | Average wall time in game `draw()` per presented frame, in milliseconds, including native calls. Not GPU execution time. |
| Present | Average wall time in the host presentation call, in milliseconds. May include VSync waiting or queued renderer work. |
| Sim drop | Total simulation time discarded by elapsed-time clamping and the catch-up limit during the window, in milliseconds. |

Values use a trailing one-second window and refresh at most twice per second.
Samples belong to the window by presentation completion time; an interval
crossing its boundary retains its full duration, including a long stall. The
maximum is a window maximum, not a session record. The first presentation
establishes a baseline; `WARMUP` lasts for the following second.

`PAUSED`, `IDLE`, and `GURU` replace the numeric readout when gameplay is inactive.
Resume, relaunch, cartridge exit, rescan and Guru recovery start a fresh window;
menu time and cartridge loading are not included as game performance samples.
There is no persistent log or cartridge-facing statistics API.

## Interpretation

At 60 presentations/sec, a typical frame interval is about 16.7 ms. A larger
maximum can reveal a hitch hidden by the average. Rising Upd suggests game logic
or its native calls deserve investigation; rising Draw suggests drawing work.
A high Present value alone does **not** establish overload: waiting for VSync is
normal. These wall times do not measure total system CPU utilization or identify
an exact function, GPU bottleneck, memory pressure, thermal state or network delay.
Other host work means the fields need not sum to Frame.

Nonzero Sim drop means the scheduler discarded elapsed simulation time rather
than performing unlimited catch-up. It can follow a stall or scheduling delay;
it is not proof that the game alone caused it. Lua instruction-budget errors
remain a separate limit, not predicted or relaxed by this readout.

Use the [adjustable-load example](../../examples/stats-demo/README.md), then compare
representative scenes with fewer objects or less detail. The author owns those
choices. SugarBee does not change game quality, timing limits, or hardware clocks.
Compare like-for-like builds on the same machine. Check again with stats disabled
because measurement and overlay drawing add some overhead.

Implementation uses a bounded 4096-sample ring (about 192 KiB). If more samples
arrive within a second, `SAMPLE LIMIT` replaces potentially misleading numbers
until a full window has passed without overflow. No per-frame allocation is
required. Extra clock reads and overlay rendering are disabled without `--stats`.

## Validation

Automated coverage includes synthetic window arithmetic/refresh/stalls/overflow,
scheduler discarded time, CLI parsing/help, inactive gameplay classification,
real renderer overlay isolation, and adjustable-load cartridge smoke tests.
Desktop automated validation passed on 2026-11-20: **188/188 serial tests**.
The user confirmed all desktop manual checks passed on 2026-11-20: readout and
load controls, pause/resume/exit/relaunch, disabled overlay, Pong and smaller-canvas
rendering. Pi acceptance remains pending.

# Developer stats demo

A bounded, adjustable workload for comparing timing fields. It is not a machine
benchmark and does not guarantee that any level will cause visible slowdown.

```sh
./build/sbee-pack -i examples/stats-demo -o build/stats-demo.vcart -t "Stats Demo" --uuid ac727fbd-e307-4363-8b80-8b8b7d36a6c9
./build/sugarbee --stats build/stats-demo.vcart
```

A/J cycles 50, 400 and 1600 objects. B/K switches between extra rectangle drawing
and extra update calculations. Both modes still draw the objects. Cycling back
to level 1 demonstrates an author-controlled reduction in workload.
See [field definitions and interpretation](../../docs/development/stats.md).

Desktop and Pi acceptance:

1. After WARMUP, confirm three readable lines updating about twice per second.
   Cycle load and work type; compare Upd/Draw without requiring a specific FPS
   or slowdown. Values should be finite and nonnegative.
2. Pause: PAUSED replaces values. Resume: WARMUP then fresh values. Exit Game:
   IDLE. Relaunch: level 1 and a fresh window. Normal host exit still works.
3. Launch without `--stats`: no overlay; demo controls still work.
4. Run Pong and a smaller-canvas cartridge with `--stats`: normal gameplay,
   scaling and pause/resume; overlay stays in host coordinates.
5. Run the serial test suite.

Desktop automated validation: **188/188 serial tests passed**, 2026-11-20,
including all six workload settings with production Lua bindings and budgets.

The user confirmed all desktop manual checks passed on 2026-11-20.
On 2026-11-20, the user confirmed Pi acceptance at `f5f9072`: **188/188 tests**
and all five requested validation checks passed. Desktop and Pi acceptance
are complete.

# Lua execution-limit checks

This checkpoint limits main-thread top-level `main.lua` execution and `init()`
to 10,000,000 Lua instructions each, and each `update()` and `draw()` to 1,000,000 per call.
These provisional budgets are independent, not wall-clock limits or a sandbox.
File reading, compilation, shutdown, native calls, coroutine handling,
and deliberate hook/error-catching bypasses remain outside this checkpoint.

From the repository root:

```sh
./build/sbee-pack -i tests/execution-limit -o /tmp/execution-limit.vcart -t "Execution Limit Test"
./build/sugarbee /tmp/execution-limit.vcart
```

Set `test_case` in main.lua and repack for each case:

- `runaway`: a nested ordinary infinite loop in update must show Guru with
  `Lua execution limit exceeded in update()`. B/K returns to the host.
  No `shutdown entered` message should appear for this failed session.
- `normal`: wait for `120 updates completed`. No Guru should appear: each call
  gets a fresh budget even though cumulative work exceeds it. Pause, resume,
  and Exit Game should work; normal exit prints one shutdown message.
- `caught_error`: update catches the hook error and returns. The host must
  still show the execution-limit Guru rather than continue the session.
  This does not test repeated catching, coroutine, or debug-hook bypasses.
- `main_runaway` and `init_runaway`: expect the execution-limit Guru naming
  `main.lua` or `init()` respectively. B/K recovers; shutdown must not run.
- `main_caught_error` and `init_caught_error`: catching the hook error and
  returning still fails startup. In the main case, init must not run afterward.
- `startup_normal`: the top-level script and init each perform work larger
  than an update allowance. Both completion messages and then `120 updates
  completed` should appear without Guru. Normal exit calls shutdown once.

- `draw_runaway` and `draw_caught_error`: Guru names `draw()` and B/K
  recovers. No shutdown message should appear. The caught case must fail
  even though its protected loop returns to the drawing function.
- `draw_normal`: wait for `120 draws completed`, with no Guru. Pause/resume
  and normal exit work; shutdown prints once. Drawing rate is not fixed, so
  this marker may take longer than the corresponding update marker.

For recovery testing, place the runaway cartridge beside Pong in a launcher
directory. Trigger the limit, recover with B/K, then launch and play Pong.
Repeat the failed launch and recovery for update, draw, and both startup stages.
Also test a single-cartridge directory: automatic launch fails once, then B/K
returns to the launcher without automatically retrying. Verify Pac-Man and
Invaders still play without execution-limit errors. Repeat on the Pi before settling the budget.
Restore `test_case` to `runaway` after testing.

The stand-in cartridge smoke harness does not exercise this native host hook.

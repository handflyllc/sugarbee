# Lua update execution-limit checks

This checkpoint limits only main-thread `update()` execution to a provisional
1,000,000 Lua instructions per call. This is not a time limit or a sandbox.
Loading, init, draw, shutdown, native calls, coroutine handling, and deliberate
hook/error-catching bypasses are outside this first checkpoint.

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

For recovery testing, place the runaway cartridge beside Pong in a launcher
directory. Trigger the limit, recover with B/K, then launch and play Pong.
Repeat the failed launch and recovery. Verify Pac-Man and Invaders still play
without execution-limit errors. Repeat on the Pi before settling the budget.
Restore `test_case` to `runaway` after testing.

The stand-in cartridge smoke harness does not exercise this native host hook.

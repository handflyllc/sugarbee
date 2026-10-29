# Interrupted save and recovery tests

Run from the repository root:

```sh
cmake --build build
ctest --test-dir build -R '^save_interruption_' --output-on-failure
```

Three native CTest cases each have an external 15-second timeout. Each uses its
own temporary directory, a known good initial save, and distinct binary payloads.
The writer runs in a child process. Test-only linker wrappers perform real I/O,
then stop the child at the requested point. The parent verifies that stop,
kills the child with SIGKILL, and reaps it. No writer cleanup runs. If the writer
finishes without reaching the point, the test fails.

| Case | Exact stop point | Expected readable save | Abandoned temporary file |
| --- | --- | --- | --- |
| `save_interruption_partial_write` | After writing seven bytes of the new payload | Complete previous value | Seven new bytes |
| `save_interruption_file_synced` | After successful temporary-file fsync, before close/replacement | Complete previous value | Complete new payload |
| `save_interruption_replaced` | After successful rename, before directory fsync | Complete new value | None |

A fresh child process reads and compares every byte of the expected save, then
writes a third value successfully. Another fresh process loads and verifies
that recovery save. The tests also check temporary-file contents before and
after recovery: abandoned files are ignored, do not block this next write,
and are not automatically removed by production code. Successful tests remove
only their own temporary fixtures. A failing or timed-out test may leave its
`/tmp/sugarbee-save-interruption-*` directory for inspection.

No engine code changes or production interruption controls are needed. These
cases use the existing-directory C writer, not first-save directory creation,
Lua lifecycle dispatch, or concurrent writers. Existing separate tests cover
normal directory preparation, injected errors/retries, and Lua bindings.
The existing temporary-name collision tests cover bounded collision retries;
these tests do not establish unlimited tolerance for accumulated leftovers.

All three cases passed on the development machine. They prove process-
interruption behavior while Linux and its filesystem caches remain running.
In particular, seeing the new value after rename does **not** establish that
it survives a power loss before directory synchronization. No save-success
result is returned by the killed child at any of the three points.

Pi validation and actual power-loss testing remain deferred. Automated removal
of abandoned files is not introduced by this checkpoint.

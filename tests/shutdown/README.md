# Manual shutdown regression test

This cartridge checks the optional Lua `shutdown()` callback. It is a manual test and is not run by CTest.

Set `test_case` in `main.lua` to one of the values below. From the repository root, repack and launch for each case:

```sh
./build/sbee-pack -i tests/shutdown -o /tmp/shutdown-test.vcart -t "Shutdown Test"
./build/sugarbee /tmp/shutdown-test.vcart
```

Close the window to finish each test.

| Case | Expected behavior |
| --- | --- |
| `normal` | Closing prints “Shutdown callback entered” and “Shutdown callback completed” once each. |
| `absent` | Closing produces no callback messages or shutdown error. |
| `shutdown_error` | Closing prints “Shutdown callback entered,” then a shutdown error report. No completion message appears. |
| `init_error` | Guru Meditation appears during initialization. Closing produces no callback messages. |
| `update_error` | Guru Meditation appears during update. Closing produces no callback messages. |

Every case should exit normally after the window is closed.

Restore `test_case = "normal"` after testing. Repack whenever the Lua source changes.
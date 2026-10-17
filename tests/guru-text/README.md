# Manual Guru Meditation text test

This cartridge tests error-message wrapping and overflow reporting. It is a manual test and is not run by CTest.

Set `test_case` in `main.lua`, then package and launch from the repository root:

```sh
./build/sbee-pack -i tests/guru-text -o /tmp/guru-text.vcart -t "Guru Text Test"
./build/sugarbee /tmp/guru-text.vcart
```

| Case | Expected behavior |
| --- | --- |
| `long_message` | The stored message wraps within the border. The yellow continuation notice appears. Console output includes `END OF MESSAGE`. |
| `long_word` | The uninterrupted X characters split across lines. The complete message fits without a continuation notice. |
| `vertical_overflow` | Displayed text stops before the continuation notice. The notice appears, and console output includes every row and `END OF MESSAGE`. |

Close the window after each test. Repack after changing the case.

The current screen copy holds at most 255 bytes. Message rendering uses up to 72 characters per line and 19 lines in the fixed 1280×720 viewport. A continuation notice appears when either storage or display space omits text.

The original diagnostic is written to standard error before the screen copy is shortened. Persistent log-file management is not implemented.

Leave `test_case = "long_message"` after testing.
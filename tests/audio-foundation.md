# Silent audio foundation checks

```sh
cmake --build build
ctest --test-dir build -R '^audio_host_' --output-on-failure
```

Each case has a 10-second CTest timeout and uses dummy video/audio drivers:

- `audio_host_silence`: checks the real callback zeros every requested byte
  without touching boundary guards, and confirms the dummy device invokes it.
  Two simulated cartridge sessions keep the same device. Lua shutdown runs
  before device close; no further callbacks occur after teardown.
- `audio_host_unavailable`: selects a deliberately nonexistent audio driver.
  Graphics/input and host startup still succeed, without Guru; teardown is safe.
- `audio_host_open_failure`: a test-only wrapper rejects device opening after
  successful subsystem initialization. The subsystem reference is released,
  host startup succeeds, and teardown does not close an invalid device.

The failure cases each produce one `Audio unavailable: ... continuing silently`
message. The wrappers and callback observer are confined to the test binary.
Dummy-driver tests do not establish hardware compatibility or audible quality.

## Interactive regression checks

Run Pong and Save Lab normally. Expect silence: the host never requests the diagnostic tone; no Lua audio API exists.
Check play/change counter, pause, resume, exit to host, relaunch, and clean host
exit. In Save Lab, check explicit saving and reloading as before.

For unavailable-audio fallback, launch from the repository root with a temporary
per-command environment override (works in fish and Bash):

```sh
env SDL_AUDIODRIVER=sugarbee-nonexistent-driver ./build/sugarbee --save-dir ./build/save-lab/saves ./build/save-lab/save-lab.vcart
```

Expect exactly one explanatory diagnostic and otherwise normal behavior, with
no Guru or repeated messages. The override does not persist for later commands.
Test console mode with the existing `--console` flag if desired.

## Validation record

All 40 automated tests passed. The user confirmed the interactive Pong and Save
Lab checks passed, and unavailable audio produced one message without disrupting
gameplay. On Pi commit `d6c0dd9`, the user confirmed all 42 tests passed, clean
tone output, normal gameplay and fallback behavior, and Exit to system status 0.
The Pi required selecting HDMI instead of its selected A/V output; see the
[test-tone troubleshooting notes](audio-tone.md#pi-troubleshooting-no-audible-tone).

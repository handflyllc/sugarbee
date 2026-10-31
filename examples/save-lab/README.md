# Save Lab

A small counter demonstrates explicit save/load using the real SugarBee API.
The counter ranges from 0 to 999999. It loads on startup, but saves only when
A/J is pressed. There is deliberately no shutdown save.

From the repository root:

```sh
cmake --build build
bash tests/save-lab/prepare-fixture.sh
./build/sugarbee --save-dir ./build/save-lab/saves ./build/save-lab/save-lab.vcart
```

The script always uses UUID `7a09c254-7fd8-4a87-9c44-742d33028ac1`.
Re-run it after editing the cartridge; it preserves existing saves. The isolated
save directory is created by the first successful save, not by preparation or
loading. Everything generated stays under ignored `build/save-lab/`. Unlike
`/tmp`, this location normally survives reboot, but deleting the build directory
also deletes this lab's saves. It is test storage, not a backup location.

| Control | Action |
| --- | --- |
| Left/Right or keyboard A/D | Change counter once per press |
| Gamepad A / keyboard J | Save current counter |
| Gamepad B / keyboard K | Reload saved counter |
| Start / Enter | Existing host pause/exit menu |

Missing saves start at zero. Reload failure or invalid data leaves the current
counter unchanged. Valid data is one to six decimal digits in the supported
range. The screen reports success, missing/invalid data, or storage failure;
uncertain durability explicitly warns that new data may already be present.

## Manual checkpoint

1. On a fresh test directory, launch: counter is zero and status says no save.
2. Press Right three times, then J: counter is 3 and status is Saved.
3. Press Right twice, then K: counter returns to 3 and status is Loaded.
4. Change without saving, use Start/Enter to exit to the host, then relaunch:
   counter is still 3. Pause/resume must not change it or trigger a save.
5. Exit the host, rerun the fixture script, and launch again: counter is 3.
6. Save zero and reload it; zero must be accepted as saved data.

For another fresh run, choose a new dedicated `--save-dir` path. Do not remove
unrelated saves. Optional invalid-data check: with the host closed, replace only
this lab's UUID `.sav` file with `invalid`, then relaunch. Expect zero and an
invalid-data message, not Guru. Press J to replace it with valid counter data.

`save_lab_behavior` tests cartridge logic with a stand-in storage API, including
errors and invalid data. `host_save_storage` separately tests real bindings and
storage. The manual checks above establish interactive end-to-end behavior on
the desktop and Pi; automated tests do not replace those checks.

## Validation record

The user confirmed all six desktop checks passed. On the Pi at `525e215`, all
37 automated tests and eight interactive checks passed: fresh startup, explicit
save, reload, pause/resume, relaunch without saving, repack persistence, saved
zero, and confirmed Exit to system returning status 0. These results do not
include actual power-loss testing.

On Raspberry Pi OS Lite 64-bit at `51cbc8b`, the user confirmed explicit save
and reload, relaunch persistence after unsaved edits, persistence through
repacking, and confirmed Exit to system status 0. All 61 automated tests passed.
This validation does not include actual power-loss testing.

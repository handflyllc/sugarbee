# Launcher manual regression checks

Build and run the existing automated suite:

```sh
cmake --build build && ctest --test-dir build --output-on-failure
bash tests/launcher/prepare-fixtures.sh
```

The fixture script requires the built packager and its dependencies. It creates
a fresh `/tmp/sugarbee-fixtures.*` directory without deleting previous runs and
prints ready-to-use launcher commands. Re-run it after reboot if temporary files
were removed. It packages the current shutdown-test source without modifying it;
use `normal` for the baseline checks. Packages retain the source case used when
packed, even if the Lua source is later changed.

## Startup and scanning

- No arguments: idle screen with launch instructions; A/J and Start do nothing;
  B/K closes the host.
- A direct Pong path: play, pause, exit to host, A/J relaunch, B/K host exit.
- Missing `--cartridges` argument or unknown option: usage message, no window.
- Use the printed `scan` command: expect 3 eligible, 2 rejected, 3 stored.
  Pong, Invaders, and Shutdown Test appear; the empty invalid header and package
  without `main.lua` are rejected with reasons in the console. The nested copy
  and text file are ignored.
- Use a nonexistent directory: console error, empty list message, B/K exit.
- Use the printed `empty` command: zero entries; directions and A/J are harmless.

## Selection and sessions

- Use the printed `scroll` command: ten entries with distinct filenames and
  duplicate titles. All ten are reachable through the eight-row display.
- Check first/last bounds, both controllers, keyboard arrows and W/S. Holding
  directions does not auto-repeat. Discovery order is not guaranteed.
- A/J launches the selected entry. A simultaneous confirmation and direction
  activates the existing selection rather than moving it first.
- From the `scan` list, launch Pong, exit, launch Invaders, and exit again.
  The list selection persists; each game starts with fresh state.
- Hold a direction during launch: movement remains blocked until release and a
  new press. Confirm the launch press does not also trigger a game action.
- Launch the normal shutdown test repeatedly. Each session prints its init
  message and one pair of shutdown messages on exit. Closing the idle host
  produces no additional shutdown callback.

## Error recovery

Temporarily select `init_error` in `tests/shutdown/main.lua`, package it into the
scan directory, then restart directory mode so the new package is discovered.
Repeat with `update_error`. Restore the source to `normal` afterward and repack
when a normal test package is needed.

- Both cases pass discovery but produce Guru Meditation when executed.
- B/K returns to the list without closing SugarBee. Held B/K does not close it;
  release and press again to close.
- Neither failed case prints `Shutdown callback entered`.
- After recovery, launch a healthy cartridge and verify normal play.
- Direct-path failures recover to the direct-path idle screen.
- Existing long-message Guru tests still wrap and preserve full console output.

## Coverage boundaries

The ten existing CTest cases cover graphics, textures, assets, input, timing,
headers, and packaging; these launcher interaction checks remain manual.
Rescanning, single-cartridge autolaunch, and detailed on-screen scan summaries
are pending and should not be expected in this checkpoint.

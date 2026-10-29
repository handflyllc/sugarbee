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
- Use the printed `scan` command: expect 4 eligible, 2 rejected, 4 stored.
  Invaders, Pacman Demo, Pong Lab, and Shutdown Test appear in that order; the
  invalid header and package
  without `main.lua` are rejected with reasons in the console. The nested copy
  and text file are ignored.
- Use a nonexistent directory: console error, empty list message, B/K exit.
- Use the printed `empty` command: zero entries; directions and A/J are harmless.

## Single-cartridge startup

Using the directory printed by the fixture script, create a separate `single`
subdirectory and copy `scan/pong.vcart` into it. Launch with `--cartridges`
pointing at `single`.

- Pong launches automatically. Exit Game returns to the launcher and stays there.
- X/U rescans without launching; A/J still launches manually.
- Add an empty `invalid.vcart` beside Pong and restart the host: Pong still
  launches, with the rejected file reported in the console.
- Multiple eligible cartridges open the launcher. Empty and nonexistent
  directories retain their existing messages and safe controls.
- In another directory, package only the shutdown test's `init_error` case.
  Startup shows Guru; B/K returns to the launcher without automatically retrying.
  Restore the shutdown source to `normal` after testing.
- A scan with errors must not automatically launch a partial one-entry list.

## Rescan and summaries

- X/U refreshes the list and selects its first entry without launching.
  Holding the button performs only one scan.
- Add or remove a cartridge while the launcher is open, then rescan.
  The list and scan counts reflect the changes.
- Rescan an empty directory: navigation and confirmation remain harmless.
- The mixed scan fixture reports two rejected files.
- With 65 eligible cartridges, 64 are listed and one is reported as
  additional and not listed. All 64 listed entries remain reachable.
- Directory errors display a distinct message with details in the console.
- After rescanning, Pong still launches, pauses, resumes, and exits.
  B/K still closes the launcher.

## Selection and sessions

- Use the printed `scroll` command: ten entries with distinct filenames and
  duplicate titles. Filenames sort from `pong-01.vcart` to `pong-10.vcart`;
  all ten are reachable through the eight-row display.
- Check first/last bounds, both controllers, keyboard arrows and W/S. Holding
  directions does not auto-repeat. Titles sort case-sensitively, then filenames.
  Above capacity, only the retained subset is sorted; the subset itself depends
  on filesystem discovery order.
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
scan directory, then press X/U in the launcher so the new package is discovered.
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

Eleven native CTest cases cover graphics, textures, assets, input, timing,
headers, and packaging. With Lua 5.4 installed, eight additional cartridge
smoke tests run against a stand-in API. These do not exercise the real host
launcher; the launcher interaction checks above remain manual.

## Console-only shutdown confirmation (test action)

Run `./build/sugarbee --console` and repeat with the fixture directory using
`./build/sugarbee --console --cartridges DIRECTORY`.

- Idle B/K does not close SugarBee; Start/Enter opens the TEST ONLY panel.
- Cancel starts selected every time. A/J cancels immediately on that selection.
- Up/Down selects within the panel without moving the cartridge selection.
- B/K and Start/Enter each cancel, including after selecting Shut down (test).
- Holding Start, directions, or A does not repeatedly act. Opening with Start
  and A together does not confirm; cancellation with B and A together cancels.
- A/J on Shut down (test) prints exactly one test request, displays the
  no-power-off notice, and returns to the idle screen. The OS and host keep running.
- X/U cannot rescan while confirmation is open; A/J cannot launch a cartridge.
- Both controllers and keyboard controls work. Cancel preserves list selection.
- Empty lists, directory errors, and direct-path idle hosts can open the panel.
- In-game Start still pauses. Resume and Exit Game work; Guru B/K still recovers.
- Without --console, idle Start remains inert and B/K still closes the host.
- Window close still works, even with the panel open. Nothing powers off the OS.

`console_host_controls` checks argument parsing and idle state transitions.
Physical input, rendering, pause integration, and Guru recovery remain manual.

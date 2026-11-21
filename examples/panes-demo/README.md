# Three-pane interaction demo

A command-driven cartridge built from existing SugarBee APIs and the single-line
editor/text-wrapping helpers. No native engine changes. Default canvas: 1280x720.

```sh
./build/sbee-pack -i examples/panes-demo -o build/panes-demo.vcart \
  -t "Three Panes" --uuid 6b58f806-8162-47bc-b220-832d93e684ce
./build/sugarbee build/panes-demo.vcart
```

CTest also packages `build/panes-demo.vcart`.

## Controls and commands

Typing begins immediately and remains active in every view. Enter submits;
Escape or gamepad Start opens the host menu. Controller gameplay is unused.
Backspace/Delete, Left/Right and Home/End edit the ASCII command line. There is
no mouse input, selection, clipboard or command-history recall.

| Command | Result |
| --- | --- |
| `view all` | Restore the three-pane split |
| `view scene` | Maximize the scene |
| `view console` | Maximize the console and show more output |
| `view status` | Maximize status/help |
| `move left` / `move right` | Move the marker one station, bounded to five stations |
| `look` | Describe the room and marker position |
| `help` | List commands |

Commands ignore case and extra spaces. Unknown or empty commands produce a
response without changing the scene. Input is interpreted only by this small
command dispatcher; it does not execute Lua or operating-system commands.

The split is 960x480 scene, 960x240 console and 320x720 status. Maximized
scene/status views retain a bottom command strip and latest response so `view
all` is always reachable. This is a game-owned layout, not separate OS windows.
The active view does not select another keyboard focus: all typing goes to the
same editor. Scene state and history survive every view change.

## Responsibilities and bounds

- `app.lua`: scene state, command dispatch and a rolling 12-entry output history.
- `ui/layout.lua`: three-pane or full-canvas rectangles.
- `main.lua`: text mode, drawing, pane clipping and cached output/status wrapping.
- `ui/textline.lua` and `ui/textwrap.lua`: unchanged copies of the existing helpers;
  see [editor contract](../text-entry-demo/README.md#reuse) and
  [wrapping example](../text-wrap-demo/README.md).

The command buffer is at most 256 ASCII characters. Errors reject the entire
edit batch, retaining text/cursor and preventing submission. Only successful
submission clears the draft; drawing, layout calculation and host pause do not.
Output is wrapped when commands change it or the layout; only the newest fitting
lines are drawn. Older entries beyond 12 are discarded. There is no scrollback
control or save persistence. Relaunch starts a fresh scene and history.

Each pane is clipped independently and clipping is explicitly reset afterward.
The scene uses simple rectangles, without a glyph sheet, native character mode,
scenario loader, general window manager or adventure framework.

## Desktop and Pi acceptance

1. Launch: confirm the three-pane proportions and readable scene, console and
   status. Type `help`, `look`, `move left`, `move right`. Marker and station/move
   counts should agree. Movement stops at the first/fifth station.
2. Try `view scene`, `view status`, `view console`, then `view all`. Commands
   remain accessible in every view; scene state/history survive. Console output
   reflows to its width and stays inside its pane. Try mixed case/extra spaces,
   an unknown command and empty Enter; responses remain visible.
3. Type an unfinished command, move the cursor into its middle, then Escape to
   pause. Resume: text/cursor, marker, history and view should remain intact;
   menu keys should not enter the draft. Also try this in a maximized view.
   On desktop, switch window focus away/back during editing.
4. Fill the 256-character line, check horizontal cursor scrolling and rejection
   of additional insertion, then delete and submit. Enter must not open the host
   menu while typing. Many submissions should roll history without drawing over
   another pane or the input strip.
5. Exit/relaunch restores the initial scene, split and empty draft. Check Pong
   movement/pause and the original text-entry demo. Exit to system normally.

Automated coverage includes command normalization/refusal, bounded movement,
history/input limits, layout dimensions, real cartridge loading and drawing,
SDL typing, view changes, host pause draft preservation and session cleanup.
The generic smoke runner supplies empty text batches only; native host tests
exercise typing separately. Visual appearance and physical input remain manual
checks. Desktop automated results and manual/Pi acceptance are recorded below.

Desktop automated validation on 2026-11-21: **204/204 serial tests passed**,
including all four new pane checks. Build completed without warnings. The user
confirmed all five desktop manual checks passed on 2026-11-21. The user confirmed
Pi acceptance at `953a202` on 2026-11-21: **204/204 tests passed**, along with
the requested manual validation. Desktop and Pi acceptance are complete.

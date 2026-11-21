# Text entry demo and single-line helper

Explicit keyboard text entry, with an ASCII line editor, visible insertion cursor,
horizontal scrolling and six submitted history entries. The game owns all of
those features. No commands are interpreted. Long history entries are abbreviated
for display, but their stored strings are unchanged. The demo uses the default
1280x720 logical canvas.

```sh
./build/sbee-pack -i examples/text-entry-demo -o build/text-entry-demo.vcart \
  -t "Text Entry" --uuid 745d5ac0-2620-4c84-a380-f8ab0efbf248
./build/sugarbee build/text-entry-demo.vcart
```

| Mode | Controls |
| --- | --- |
| Movement | Left/Right or A/D moves the square; J or gamepad A starts a fresh line |
| Typing | Printable ASCII; Backspace/Delete; Left/Right; Home/End; Enter submits |
| Either | Escape or gamepad Start opens the host menu |
| Movement only | Enter also opens the host menu |

The game deliberately pauses square movement while typing; physical controller
buttons remain available through the engine. The host pause menu uses the usual
Up/Down, J/A, K/B and Start/Enter/Escape controls. Pausing preserves the line and
cursor; resume discards pending typing and suppresses held keys until release.
A fresh J press is required to begin again after submission. Exit/relaunch resets
the buffer, cursor, square and history.

## Reuse

Copy `ui/textline.lua` into your cartridge. Call `require('ui.textline').new()` to
create independent editors. Each editor has `text` and `cursor` fields (a string
and zero-based insertion position) and `editor:apply(events, status)`, which
accepts the **trusted results of `sugarbee.text_events()`**. It does not validate
arbitrary event tables or external mutations of its fields. It does not call
engine mode functions, draw, parse commands or maintain history.

`apply` returns `(submitted_text_or_nil, error_or_nil)`. Empty submission is `''`,
so test `submitted ~= nil`. Submission preserves the buffer and cursor and stops
processing the remainder of that batch. The caller chooses whether to end text
entry, clear the editor or focus another field. This demo ends entry on submission.
Ordinary edits return `nil, nil`.

The helper limits a line to 256 ASCII characters. If an insertion would exceed
that limit, `nil, 'full'` rejects the entire edit batch, preserves the previous
line/cursor and prevents submission. Delete some text before retrying. Engine
`overflow`/`unsupported_text` errors also leave the editor unchanged. The game
shows a message instead of executing or storing a partially accepted command.

The helper is single-line; text wrapping remains an independent display-layout
helper. Keyboard selection, clipboard, mouse positioning, command history recall,
Unicode input/composition and multiline editing are outside this checkpoint.
See the [engine contract](../../docs/api/input.md#explicit-text-entry).

## Desktop and Pi acceptance

1. In movement mode, check arrows/A/D. Enter and Escape open the host menu.
   Resume and verify movement. J or controller A begins typing without inserting
   the activation key. Release activation/menu keys before typing.
2. Type upper/lowercase letters, digits, spaces and punctuation. Verify J/K/U/I
   and W/A/S/D type normally without game actions. Check Backspace/Delete,
   Left/Right, Home/End and platform key repeat. Enter submits exactly once and
   restores movement. Empty submission works. The demo retains six history lines.
3. Hold A or D while pressing Enter to submit. The square must not move until
   that letter is released and pressed again. Holding Enter must not immediately
   open the pause menu after submission. Gamepad Start must still pause while
   typing (if a controller is available).
4. Pause mid-edit using Escape, resume, and confirm text/cursor are preserved
   and menu keys did not enter the field. On desktop, also switch focus away and
   back while editing. No stuck keys or replay of pending typing should occur.
5. Fill a line to 256 characters (holding a printable key is sufficient where
   platform repeat is available), verify the visible error on further insertion,
   then delete and submit. Cursor remains visible as the line scrolls. If your
   layout can enter non-ASCII text, verify rejection without changing the line.
6. Exit/relaunch starts fresh. Check Pong movement/pause and the text-wrap demo;
   their existing controls should still work, with Escape added as a pause key.
   Normal host exit still works.

Automated validation covers native SDL event ordering, repeat, buffering, batch
limits, unsupported text, focus and mode transitions, controller coexistence,
Lua arity/callback/result contracts, actual module loading and demo callbacks,
pause/edit preservation, full-buffer rollback, Guru/exit/relaunch cleanup and
single-line editing. The smoke runner supplies **empty text batches only**;
it does not model typing, keyboard suppression or host pause behavior.
Desktop automated validation on 2026-11-21: **200/200 serial tests passed**,
warning-free build and native input UBSan checks passed. The user confirmed all
desktop manual checks passed on 2026-11-21. The user confirmed all manual Pi
checks passed at `44e7c47` on 2026-11-21. Pi acceptance is complete using the
initial 189 passing tests plus the successful 11/11 focused serial recheck;
this does not represent a single clean 200-test Pi run.

The Pi suite timed out at `lua_modules_pack` (10.06 seconds, 10-second limit),
blocking ten dependent module checks. Native text-entry tests passed. The later
standalone `pi-modules-trace.log` shows fixture generation and packaging completed
successfully (44041 ZIP bytes), but contains no elapsed timing. This does not
establish the original cause or diagnose storage failure. The affected CTest
group subsequently passed in `pi-modules-recheck.log`: 11/11 tests, 10.57 seconds
total, with the fixture taking 8.76 seconds against its 10-second limit. The
small timeout margin warrants follow-up, but the original delay remains
unexplained. No timeout or runtime changes were made.

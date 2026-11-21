# Text wrapping helper and demo

Copy `ui/textwrap.lua` into your cartridge's `ui/` directory:

```lua
local textwrap = require('ui.textwrap')
local lines, height = textwrap.wrap('A message for the player.', 240, 2)
-- Cache layout until text, width or scale changes.
for i, line in ipairs(lines) do
    sugarbee.print(line, 16, 48 + (i - 1) * 20, 255, 255, 255, 2)
end
```

This is a reusable cartridge-local Lua helper, not a new built-in engine API.
Width is logical pixels. Scale defaults to 1 (omitted or nil); width and scale
must be numeric positive integers no greater than 2147483647. Numeric strings
are rejected. Width must fit at least one 8-pixel-wide scaled character cell.
The existing measurement API also rejects overflowing pixel dimensions.

Text must be a string, at most 4096 bytes, without embedded NUL. Output is at
most 256 lines. Invalid arguments or exceeded limits raise Lua errors; no partial
layout is returned. Catch with `pcall` if recovery is appropriate for your game.
The helper returns a fresh dense table of strings and total character-cell height:
`(8 + 10 * (line_count - 1)) * scale`. Empty source returns `{}`, 0. A nonempty
spaces-only source returns one blank line with height `8 * scale`.

Ordinary ASCII spaces collapse between words and disappear at line edges.
Explicit newlines preserve blank and trailing lines. Words that exceed the full
line width split into byte-sized cells; a word that fits a fresh line moves there
whole. Tabs, carriage returns and non-ASCII bytes keep the renderer's individual
fallback cells. UTF-8 is not decoded; this is not Unicode-aware layout. No CRLF
normalization, hyphenation, alignment, automatic clipping or pagination is added.
Keep original source text separately from this display layout for future editing.

Layout is intended for initialization or when content/settings change, not every
frame. Tests sample Lua instruction counts for maximum-size inputs; this does not
establish a native timing guarantee or reserve the callback's budget for the helper.
Drawing, visible-line selection, scrolling and retained layout belong to the game.
Keyboard entry is separate future work.

## Build and manual checks

```sh
./build/sbee-pack -i examples/text-wrap-demo -o build/text-wrap-demo.vcart \
  -t "Text Wrap" --uuid c9eb5017-a4ab-49ef-9386-b0068eff4d12
./build/sugarbee build/text-wrap-demo.vcart
```

1. J/A cycles normal prose, explicit blank/trailing lines, a long word, extra
   spaces and empty text. Text stays within the outlined width. Empty source
   reports height zero; the demo still draws a minimum-height outline.
2. K/B toggles narrow/wide and U/X toggles scale 1/2. Words reflow and the outline
   follows total cell height; blank lines keep their vertical space.
3. Pause/resume, exit and relaunch work normally; relaunch resets the demo.
4. Pong and the text-measurement demo still render normally; host exit works.

The helper has focused layout/error/limit tests and the demo runs in the modeled
smoke runner. Native text metrics remain covered by existing C tests. Desktop
automated validation passed on 2026-11-21: **195/195 serial tests**. Desktop
manual acceptance was confirmed by the user on 2026-11-21: all four requested
checks passed. Pi acceptance remains pending; no new renderer behavior is implemented.

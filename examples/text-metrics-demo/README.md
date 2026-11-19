# Text measurement demo

A 320×240 canvas shows the same sample text at scales 1 and 2, centered using
`sugarbee.measure_text()`. Cyan outlines sit just outside measured character
cells; the yellow labels show dimensions, including spaces and blank lines.
See the [measurement contract](../../docs/api/graphics.md#text-measurement).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/text-metrics-demo \
  -o build/text-metrics-demo.vcart -t "Text Metrics Demo"
./build/sugarbee build/text-metrics-demo.vcart
```

## Manual checks (desktop, then Pi)

1. `Paused` is centered in each cyan outline. Upper dimensions are 48×8,
   lower dimensions are 96×16. Font shapes and spacing look unchanged.
2. A/J cycles through five samples. `Two\nlines` measures 40×18 at scale 1;
   the lower sample doubles both dimensions. Neither line escapes its box.
3. ` A  ` includes its leading/trailing spaces: 32×8 at scale 1. The visible
   letter is deliberately not centered within those cells.
4. `End\n` leaves room for a blank second line (24×18 at scale 1).
   The final sample is just a newline: a narrow, empty outline representing
   zero width and an 18-pixel height (36 at scale 2).
5. Pause/resume, Exit Game/relaunch and Exit to system work normally.
   Check canvas-demo and Pong still render normally.

## Automated coverage

Native tests check cell dimensions, scale, whitespace, newlines, byte fallback,
NUL termination, explicit byte spans, input limits, overflow and error outputs.
Production Lua checks argument counts, default scale, numeric text conversion,
exact result count and validation. Pixel/state checks verify measurement neither
renders nor changes camera, clipping, tint or target, and that rendered multiline
text fits the measured cells. The existing font regression protects glyphs.
The smoke model has matching measurement checks and runs this demo for 600 frames.

Desktop: **165/165 serial CTests passed**, with native text metrics and font
checks also passing UBSan. The packaged demo ran headlessly for two seconds
without reporting a Guru error; the harness terminated it. On 2026-11-18, the
user confirmed all desktop checks passed: measured boxes, sample cycling,
Pause/resume, Exit Game/relaunch, canvas-demo/Pong and Exit to system.
Pi acceptance remains pending.

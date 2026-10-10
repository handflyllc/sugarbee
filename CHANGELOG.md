# Changelog

## Unreleased (SugarBee 0.1 baseline)

- Added SDL2 gamepad discovery/hotplug handling behind a language-independent
  `sb_input.h` interface, plus developer keyboard fallback for player zero.
- Kept Lua `sugarbee.btn()` and added `sugarbee.btnp()` for pressed edges;
  added headless input smoke tests, a moving-box demo, and input documentation.

- Extracted Lua-independent 2D drawing services into `sb_graphics.h` and an
  SDL2 backend; preserved existing `cls`, `rect`, `print`, and Guru rendering.
- Added a headless graphics smoke test and synchronized graphics/architecture
  documentation. Input and timing were not part of that earlier graphics change.

- Added CMake build and CTest baseline for the current C host and packager.
- Restored bitmap-font data from the historical template (partial ASCII font).
- Added explicit little-endian cartridge header encoding/decoding and full
  minimum version validation (major, minor, patch).
- Reworked the Linux packager to create ZIPs without invoking a shell and
  added random UUIDv4 generation plus `--uuid` reuse for game save identity.
- Added build, dependency, and cartridge-format documentation.

Not a release: fixed-step timing, Lua sandboxing, analog input, and cartridge
resource limits remain outstanding.

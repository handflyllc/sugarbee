# Changelog

## Unreleased (SugarBee 0.1 baseline)

- Added CMake build and CTest baseline for the current C host and packager.
- Restored bitmap-font data from the historical template (partial ASCII font).
- Added explicit little-endian cartridge header encoding/decoding and full
  minimum version validation (major, minor, patch).
- Reworked the Linux packager to create ZIPs without invoking a shell and
  added random UUIDv4 generation plus `--uuid` reuse for game save identity.
- Added build, dependency, and cartridge-format documentation.

Not a release: engine input, timing, Lua sandboxing, and cartridge resource
limits remain outstanding.

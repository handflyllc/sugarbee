# Cartridge asset API (experimental SugarBee 0.1, Milestone 3B)

SugarBee cartridges remain a **64-byte SBEE header followed by ZIP payload**.
An asset is a named ZIP entry, such as `assets/bee.png`, rather than an OS
filesystem path. Game authors package their `main.lua` and `assets/` directory
together with `sbee-pack`.

## Game author interface

```lua
function init()
    bee = sugarbee.texture_load("assets/bee.png")
end
```

`sugarbee.texture_load(path)` returns a texture handle compatible with
`sugarbee.sprite(handle, x, y)` and `sugarbee.texture_free(handle)`. It
supports PNG files only. Decode/upload once during `init()` where possible;
repeated calls allocate distinct textures and consume memory.

Invalid names, missing files, invalid PNGs and resource-limit violations raise
Lua errors. As with other Lua runtime errors, SugarBee displays Guru Meditation.
Pixel buffers are not exposed to Lua; asset bytes and decoded pixels are freed
after upload. Persistent texture handles are owned by the graphics context.

## Resource validation and limitations

SugarBee 0.1 applies these provisional hard limits in the cartridge reader:

| Resource | Current limit |
| --- | ---: |
| Compressed ZIP payload | 64 MiB |
| Individual uncompressed entry | 16 MiB |
| Total declared uncompressed entries | 128 MiB |
| ZIP entries | 1024 |
| Asset member path | 240 bytes |
| PNG width and height | 2048 pixels each |

Member names must be relative, forward-slash-separated paths without empty
segments, `.` or `..`, colons, control bytes, or backslashes. Duplicate exact
names are rejected; ZIP CRC is checked while reading the requested member.
The whole directory is scanned before an asset is accepted.

The current host reads `main.lua` through the same bounded reader as assets.
minizip 1.x is used on the prefixed cartridge file. This has been exercised on
Linux, but compatibility with other minizip versions and the Pi 4 environment
still needs testing. The reader does not unpack ZIP members onto the host
filesystem; it allocates memory and reads only the selected entry.

These checks are defensive limits, **not** a security sandbox. Lua currently
opens standard `io` and `os` libraries, and game-supplied Lua remains trusted
code in the current prototype. A complete untrusted-cartridge security model
and per-game memory budget are future work.

## Implementation boundaries

- `include/sb_assets.h`, `src/sb_assets.c`: ZIP-backed bounded asset access,
  with allocated binary buffers returned to the caller.
- `include/sb_png.h`, `src/sb_png.c`: PNG decoding to packed RGBA8 bytes using
  libpng's simplified memory API.
- `include/sb_graphics.h`: receives the decoded pixels through the preexisting
  `sb_graphics_texture_rgba()` service, which knows nothing about ZIP or PNG.
- `src/main.c`: small Lua adapter function that joins these independent pieces.

This is an internal service design; native-game interfaces and their ABI are
not yet defined.

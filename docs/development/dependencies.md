# Dependencies (SugarBee 0.1 prototype)

Primary development environment: **Debian 12 (Bookworm), x86_64**.
Initial deployment target: **Raspberry Pi 4, AArch64**, using Linux.

Install build dependencies on Debian 12:

```sh
sudo apt update
sudo apt install --no-install-recommends \
  build-essential cmake pkg-config \
  libsdl2-dev liblua5.4-dev libminizip-dev libpng-dev zlib1g-dev zip
```

| Package | Why it is needed |
| --- | --- |
| `build-essential` | C99 compiler and linker |
| `cmake` | Build-system generator (minimum CMake 3.16) |
| `pkg-config` | SDL2, Lua, and minizip discovery |
| `libsdl2-dev` | Current host graphics/windowing/gamepad API |
| `liblua5.4-dev` | Embedded Lua 5.4 VM and headers |
| `libminizip-dev` | ZIP reader for bounded cartridge asset access (`sb_assets.c`) |
| `libpng-dev` | PNG image decoding from cartridge memory (`sb_png.c`); no SDL_image required |
| `zlib1g-dev` | Compression support |
| `zip` | External CLI currently used by `sbee-pack` |
| `lua5.4` (optional) | Standalone interpreter for the headless `cartridge_smoke` tests; CMake skips them when it is absent |

No KDE, X11 development stack, desktop environment, Node.js, or npm dependency
is required by SugarBee's current C build. SDL2 may have desktop-oriented
transitive dependencies in the Debian development environment; the final
console image will be addressed separately.

The standalone cartridge parser and packager can be built without SDL2/Lua or PNG:
configure with `-DSUGARBEE_BUILD_HOST=OFF`.

Current packaging tool uses POSIX Linux facilities (`fork`, `exec`,
`/dev/urandom`); porting this *tool* to Windows is future work. The game API
itself is intended to remain platform-neutral.

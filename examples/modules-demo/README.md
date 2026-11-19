# Cartridge modules demo

A small `main.lua` delegates movement to `game.player` and text to `ui.hud`.
Both import `game.shared`, demonstrating one shared cached table.
See the [module contract](../../docs/api/modules.md).

```sh
cmake --build build -j4
./build/sbee-pack -i examples/modules-demo -o build/modules-demo.vcart -t "Modules Demo"
./build/sugarbee build/modules-demo.vcart
```

## Manual acceptance (desktop, then Pi)

1. A cyan square moves across the screen. Player X changes, and shared module
   loads stays at **1**, although multiple files import the shared module.
2. Press A: a caught missing-module message appears and movement continues.
   Repeat A to confirm explicit retries work without leaving a loading marker.
3. Pause/resume normally. Exit Game and relaunch: position restarts near 100,
   the original status returns and the load count is still 1.
4. Press B: the intentional uncaught module error enters Guru. Console details
   should name `game.broken`, `game/broken.lua` and the intentional failure.
   Exit Game and relaunch; the game starts normally again.
5. Pong and Pac-Man still work; Exit to system returns normally.

## Automated validation

Native tests use production library setup, the real archive reader and host
instruction hooks. They cover result identity, false/nil/scalar/function returns,
fresh sessions, invalid names/arity, missing files, syntax and bytecode rejection,
cycles/retries, dependency caching after parent failure, non-string errors,
exact/over source limits, failed-compilation and failed-CRC accounting,
16/17 nested imports, 128/129 cached modules with pending reservations, and
shared execution budgets. Host entry-point/init integration is checked too.

The Lua smoke-runner model uses trusted unpacked files and validates module
organization/cache behavior. It does not establish archive security or native
instruction-budget enforcement. B deliberately raises an error, so this demo is
not part of the random-input smoke list.

Desktop: **158/158 serial CTests passed**. Final 15 targeted module/library/
runner checks passed after adding empty-file and maximum-name coverage.
The build is warning-free. All nine native module modes passed ASan/UBSan with
leak detection disabled; LeakSanitizer cannot run under this traced environment,
so no leak-check result is claimed. The demo passed a native two-second
dummy-driver launch and a one-frame smoke-model run.

On 2026-11-18, the user confirmed desktop validation passed: movement/shared
cache display, caught missing-module errors, pause/resume and state reset on
relaunch, intentional Guru and recovery, Pong/Pac-Man regressions, and Exit to
system. These results cover the modules checkpoint built on `235046f`.
On 2026-11-18, the user confirmed all Pi checks passed at `93ea8a3`:
**158/158 serial tests**, movement/shared cache, caught missing-module errors,
pause/resume and relaunch reset, intentional Guru/recovery, Pong/Pac-Man
regressions and Exit to system. Desktop and Pi acceptance are complete.
No new audio listening comparison is required.

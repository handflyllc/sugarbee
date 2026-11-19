# Cartridge-local Lua modules

The host supplies `require("module.name")` for organizing code inside the active
`.vcart`. No manifest is required. This is a general-purpose module loader;
scenario formats and catalogs belong to individual games.

| Call | Exact archive member |
| --- | --- |
| `require("player")` | `player.lua` |
| `require("ui.menu")` | `ui/menu.lua` |
| `require("levels.forest")` | `levels/forest.lua` |

Names are case-sensitive ASCII identifiers separated by dots. Each segment starts
with a letter or underscore, followed by letters, digits or underscores. Names
contain 1–235 bytes. Pass exactly one string, omit the `.lua` suffix, and use dots
instead of slashes. Empty segments, path traversal, NULs, absolute paths and
backslashes are rejected. There is one mapping, with no `init.lua` fallback,
host search paths, native libraries or separately installed packages.

Modules compile as Lua source text only. The existing `main.lua` entry-point
loading contract is unchanged. The package library, `dofile` and `loadfile`
remain unavailable; this loader does not enable general filesystem access.

## Results, lifetime and organization

The first successful import executes the file with no arguments and caches its
first return value. Later imports return that exact value without reading,
compiling or executing the file again. Tables/functions retain identity.
Any non-nil value, **including false**, is cached. Nil or no return becomes true.
Extra returned values are discarded; `require` returns exactly one value.
Caching false intentionally differs from standard Lua's truthy-cache behavior.

Modules share the cartridge's global environment. Locals remain private to their
chunk or closures; globals are shared. This is code organization, not isolation.
Define functions and return a table, then perform game setup explicitly:

```lua
-- game/player.lua
local x = 0
return {
    init = function() x = 100 end,
    update = function(dt) x = x + 80 * dt end,
    draw = function()
        sugarbee.rect(math.floor(x), 200, 24, 24, 100, 210, 255, true)
    end,
}
```

```lua
-- main.lua
local player = require("game.player")
function init() player.init() end
function update(dt) player.update(dt) end
function draw()
    sugarbee.cls(20, 25, 40)
    player.draw()
end
```

The private cache and counters belong to the cartridge's Lua state. Exit,
failed startup and relaunch use the existing state teardown/new-state lifecycle;
nothing is shared with another cartridge session. No cache reset, hot reload,
discovery or public `package.loaded` interface is provided.

## Loading limits

| Limit | Value |
| --- | --- |
| Uncompressed source per module | 256 KiB (262144 bytes) |
| Cumulative module source read per session | 8 MiB (8388608 bytes) |
| Cached modules | 128 |
| Simultaneously executing nested imports | 16 |

Pending imports reserve cache capacity, so children cannot consume the slot
needed by an in-flight parent. Folder depth does not determine import depth.
Cached imports consume no further source allowance, slot or nesting depth.

Source delivered by decompression is charged even when CRC verification,
compilation or execution later fails. A missing file or an entry rejected before
reading consumes no source bytes. Failed reads may consume only the bytes read
before failure. The remaining session allowance limits the read itself.
An empty source file can still load with zero source allowance remaining.
Archive size, aggregate size, entry count, path, duplicate-member and CRC
validation from the [asset reader](assets.md) also apply.

Module execution shares the caller's existing [instruction budget](timing.md);
nested imports never grant a fresh budget. Compilation and native ZIP operations
are not counted as Lua instructions. Source and depth caps bound individual loads,
but are not a comprehensive CPU or memory sandbox. Runtime tables, strings and
other Lua allocations are not bounded by the cumulative source allowance.
The existing `main.lua` source does not count toward these new module limits.

## Failure behavior

Missing or invalid members, syntax/runtime errors, binary chunks, cycles and
exceeded limits raise Lua errors. String errors identify the importing module and
retain the underlying error; compiler diagnostics retain the member filename and
line. Circular imports report a dependency chain. Non-string error objects are
re-thrown unchanged, so `pcall` preserves object identity; allocation errors
are also propagated without allocating a new descriptive wrapper.

Uncaught errors follow the existing Guru/lifecycle handling. A game may catch an
ordinary import error with `pcall`; catching an instruction-limit error does not
erase the host's execution-limit flag.

Failed modules are not cached. Their in-progress marker and pending capacity are
released, allowing a later explicit retry; there are no automatic retries.
Source allowance already spent is not refunded.

Loading is not transactional: successful dependencies stay cached if a parent
fails, and earlier changes to globals, tables or engine state remain. Keeping
initialization explicit avoids accidental repeated side effects during retries.

See the [multi-file demo and validation checklist](../../examples/modules-demo/README.md).

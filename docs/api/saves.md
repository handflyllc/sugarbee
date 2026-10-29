# Save data

SugarBee stores one opaque string per cartridge UUID, up to **65,536 bytes**
(64 KiB). Empty strings and embedded zero bytes are valid. The cartridge owns
serialization and versioning; the engine neither interprets the data nor
serializes tables. Retain the UUID when repacking to keep using the same save.

## `sugarbee.load_save()`

Takes no arguments and returns exactly two values:

| Results | Meaning |
| --- | --- |
| `data, nil` | Loaded a string, including a possible empty string. |
| `nil, "not_found"` | No save file or directory exists. Use game defaults. |
| `nil, "too_large"` | The stored file exceeds 65,536 bytes. |
| `nil, "io_error"` | Storage is unavailable, the path/file is unsuitable, or reading failed. |

Loading never creates directories or files. A missing save is not an error
message in the console; other storage failures include console diagnostics.

## `sugarbee.save(data)`

Takes exactly one **Lua string**. Numbers are not automatically converted:
use `tostring(score)` when that is your intended format. Wrong types, missing
arguments, or extra arguments raise a normal Lua error, catchable with `pcall`.
Passing any argument to `load_save()` also raises a Lua error.

Returns exactly two values:

| Results | Meaning |
| --- | --- |
| `true, nil` | Replacement and required file/directory synchronization succeeded. |
| `nil, "too_large"` | Input exceeds 65,536 bytes; the previous save is unchanged. |
| `nil, "io_error"` | Saving failed before replacement; the previous save is unchanged. |
| `nil, "durability_uncertain"` | Replacement occurred but final synchronization or directory close failed. New data may be visible; persistence is not confirmed. |

Save prepares missing directories and replaces the file through a synchronized
temporary file in the same directory. It never truncates oversized data or rolls
back after replacement. Treat `durability_uncertain` separately from failure
before replacement: do not assume the previous save is still present.

## Lifecycle, location, and limits

Both calls are available in `init()`, normal gameplay callbacks, and eligible
`shutdown()`. At top-level `main.lua`, before the cartridge has finished loading,
valid calls return `nil, "io_error"`. Failed sessions cannot use storage;
Guru cleanup skips the shutdown callback. Invalid arguments still raise errors.

The host chooses the directory using `--save-dir`, XDG, or HOME; see the
[launcher rules](../development/launcher.md#save-directory-selection).
Lua cannot choose a path or UUID. Cartridges with the same UUID share a save;
this is identity organization, not authentication.

Calls are synchronous and may stall a frame. Save at deliberate checkpoints
or menu actions, not every frame. Nothing saves automatically. Shutdown is an
extra opportunity, not a substitute for checkpoints. Lua memory allocation
errors remain ordinary Lua errors rather than storage result codes.

```lua
function init()
    local data, code = sugarbee.load_save()
    if data ~= nil then
        score = tonumber(data) or 0
    else
        score = 0
        if code ~= "not_found" then
            print("Could not load score: " .. code)
        end
    end
end

-- Call at a deliberate checkpoint; this function is not an engine callback.
function save_score()
    local ok, code = sugarbee.save(tostring(score))
    if not ok then
        print("Score save result: " .. code)
    end
    return ok, code
end
```

The [save design](../architecture/save-design.md) details bounds, path resolution,
and durability expectations. Desktop regression tests cover API results,
lifecycle access, and injected failures. Interactive lab, Pi, and interruption/
power-loss validation remain pending; successful synchronization is not a
blanket guarantee against hardware failure.

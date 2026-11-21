# Save data

SugarBee stores one opaque string per cartridge UUID, up to **65,536 bytes**
(64 KiB). Empty strings and embedded zero bytes are valid. The cartridge owns
serialization and versioning; the engine neither interprets the data nor
serializes tables. Retain the UUID when repacking to keep using the same save.
The [persistent packaging identity workflow](../development/packaging-identity.md)
records it in the game source and reuses it automatically.

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
Passing any argument to `load_save()` or `delete_save()` also raises a Lua error.

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

## `sugarbee.delete_save()`

Takes no arguments and returns exactly two values. Extra arguments, including
`nil`, raise a normal Lua argument error. No filename, UUID or slot is accepted.

| Results | Meaning |
| --- | --- |
| `true, nil` | The save was deleted and required synchronization succeeded, or no save exists. |
| `nil, "io_error"` | No deletion was performed by this call; storage is unavailable, unsuitable, or an operation failed. |
| `nil, "durability_uncertain"` | Deletion occurred but directory synchronization or close failed. The save may already be absent; persistence is not confirmed. |

Deletion never creates directories. It accepts only a regular save file, rejecting
symlinks and other entry types. An oversized or game-invalid regular save can be
deleted. If the directory exists but the file is already absent, the directory
is synchronized so a retry can confirm persistence after an uncertain deletion.
Storage failures can still be reported on that retry. No rollback is attempted.

The game owns confirmation and resetting its in-memory state. Deletion removes
only the current UUID's `.sav` file, not the directory, other UUIDs, or temporary
files. It does not change running game variables or prevent a later save,
including one written by the game's `shutdown()` callback.

### Multiple game save slots

One file per UUID can contain multiple game-defined slots. The game owns slot
selection, encoding and decoding; all slots together must fit within 64 KiB.
Deleting one slot means removing it from the game's data and saving the remaining
collection. `delete_save()` erases the entire collection. Empty strings remain
valid saved data, distinct from a missing save.

## Lifecycle, location, and limits

All three calls are available in `init()`, normal gameplay callbacks, and eligible
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
lifecycle access, and injected failures. The [Save Lab](../../examples/save-lab/README.md)
provides interactive checks; the user confirmed all six desktop checks passed.
Controlled [process-interruption tests](../../tests/save-interruption.md) also
pass on desktop and Pi. On `525e215`, the user confirmed all 37 Pi tests, all
eight interactive checks, and Exit to system status 0. Actual power-loss testing
remains deferred; successful synchronization is not a
blanket guarantee against hardware failure.

Save-deletion checkpoint (baseline `00d541b`): native fault-injection, real Lua
lifecycle/argument checks and smoke/Save Lab behavior coverage are implemented.
Desktop automated validation passed on 2026-11-20: **189/189 serial tests**.
The user confirmed all desktop manual checks passed on 2026-11-20: cancellation,
deletion/repeated deletion, unchanged running counter, missing-save reload,
relaunch reset, saving again, pause/exit and Pong. On 2026-11-20, the user
confirmed Pi acceptance at `a19284a`: **189/189 tests** and all four requested
manual checks passed. Desktop and Pi save-deletion acceptance are complete.

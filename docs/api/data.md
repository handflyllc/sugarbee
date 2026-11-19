# Cartridge data reader

Implemented in `9d75cd8` against `9b2f70c`. Desktop accepted on 2026-11-18;
the user confirmed Pi acceptance on 2026-11-19: 177/177 tests and all four
requested visual, lifecycle and regression checks passed.

```lua
local bytes, err = sugarbee.read_file("assets/dialogue.txt")
```

Reads one named entry from the active `.vcart` into a Lua string. The cartridge
stays compressed on disk; each call scans its ZIP directory and decompresses
only the requested entry. There is **no engine cache**, extraction directory,
streaming interface, parser or automatic execution of the returned bytes.

## Results and arguments

Exactly one string argument is required. Paths are case-sensitive names relative
to the cartridge root, using `/` separators. There is no extension restriction.
Paths must contain 1–239 bytes; empty segments, `.`/`..`, absolute paths,
backslashes, colons, control bytes and embedded NULs are rejected as Lua argument
errors. Numeric path arguments are not converted to strings.

Every ordinary result has exactly two values:

| Result | Meaning |
| --- | --- |
| `bytes, nil` | Complete file read, including CRC verification. |
| `nil, "not_found"` | Requested member does not exist. |
| `nil, "too_large"` | Per-file, remaining session or existing archive resource limit exceeded. |
| `nil, "corrupt"` | Invalid/damaged ZIP, ambiguous requested member or failed read/CRC validation. |
| `nil, "invalid_cartridge"` | Unsupported or invalid SBEE header. |
| `nil, "io_error"` | Cartridge I/O or native buffer allocation failed. |

The string preserves all bytes: embedded zeros, line endings and binary values.
An empty file succeeds with `"", nil`; it is distinct from a missing file.
No encoding conversion, newline normalization or JSON parsing occurs.

Invalid API arguments raise Lua errors. Ordinary read failures return error
codes so games can recover or provide fallback content. A Lua allocation failure
while constructing the returned string still raises a Lua memory error; the
temporary native read buffer is freed before that error propagates.

## Limits and lifetime

| Limit | Bytes |
| --- | ---: |
| Per-file uncompressed data | 1 MiB (1,048,576) |
| Cumulative data read per cartridge session | 16 MiB (16,777,216) |

The effective file limit is the smaller of the per-file limit and the remaining
session allowance, checked before decompression/allocation using the declared
entry size. The counter charges payload bytes delivered by decompression, even
if CRC verification later fails or Lua cannot allocate the returned string.
Repeated reads count again. Missing files, invalid paths and entries rejected
before reading consume no byte allowance. Releasing a Lua string gives no refund.
After the allowance is exhausted, empty files can still succeed; nonempty files
return `nil, "too_large"`. Archive validation still applies to empty files.

The data allowance is separate from module-source limits, music/WAV budgets and
configuration loading. It is a read-work limit, **not a total Lua memory cap**.
The host's general cartridge/security limitations still apply. It also does not
bound all directory scans or provide a wall-clock I/O deadline.

The reader is available before `main.lua` executes, including to required modules,
and during init/update/draw/shutdown. A fresh cartridge Lua state resets its
counter; pause/resume does not. Existing engine behavior skips shutdown after
Guru failure. Retained strings remain ordinary Lua values until no longer
referenced; the engine does not keep another cached copy. Read at startup or
level transitions, not on every draw/update.

```lua
local dialogue
function init()
    local err
    dialogue, err = sugarbee.read_file("assets/dialogue.txt")
    if not dialogue then
        dialogue = "Dialogue unavailable: " .. err
    end
end

function draw()
    -- Uses the retained string: no file access or decompression.
    sugarbee.print(dialogue, 20, 20)
end
```

This exposes no host filesystem path, external package loading or writes.
Existing `.vcart` binary format and save identity remain unchanged. Use `require`
for Lua modules; deciding how to parse ordinary game data belongs to the game.
Bundled scenario descriptions can use this general facility without a dedicated
scenario engine API or required manifest.

## Verification

Native/production Lua checks cover argument and path validation, text/binary/
empty reads, maximum file size and member name, missing/invalid cartridges,
I/O failures, repeated reads, retained strings, partial remaining allowances,
CRC failures charging the allowance, fresh sessions and host lifecycle calls.
A fault-injected Lua allocation failure verifies native-buffer cleanup and
retained charging. The [demo](../../examples/data-demo/README.md) exercises
retained text, a binary self-check and recoverable missing-file errors.

The smoke runner models data reads from trusted unpacked directories with the
same byte limits and no cache. It does not validate ZIP/header/CRC behavior,
model native allocation failure or provide filesystem isolation.

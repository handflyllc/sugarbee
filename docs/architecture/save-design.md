# SugarBee saves — settled initial design

Status: initial API, storage policy, and durability contract agreed on 2026-10-29.
Save support and the API below are not implemented. This document records the
accepted design before implementation; it is not an available API reference.

## Scope and ownership

Provide one opaque Lua string of at most 65,536 bytes (64 KiB) per persistent
cartridge UUID. Empty strings and embedded zero bytes are valid; the limit is
in bytes, not characters. Never truncate data silently.

The cartridge owns encoding, interpretation, and save-data versioning. A first
lab can store a high score using `tostring` and read it using `tonumber`.
The engine does not serialize Lua tables or validate game-specific contents.

The host derives the save identity from the loaded cartridge's UUID. Lua cannot
supply a UUID or filesystem path. Repacking with the same UUID preserves the
association; a new UUID selects a different save. Cartridges sharing a UUID
share that identity. This is organization, not authentication.

## Lua API and results

```lua
local data, code = sugarbee.load_save()
local ok, code = sugarbee.save(data)
```

Both calls are synchronous. Expected storage failures return stable codes;
games must not parse human-readable diagnostic messages. Wrong argument types
raise normal Lua programming errors. Saving requires a string.

| Call and result | Meaning |
| --- | --- |
| `load_save()` returns `string, nil` | Successful load; an empty string is valid data. |
| `load_save()` returns `nil, "not_found"` | No save exists; the game can use defaults. |
| `save(data)` returns `true, nil` | Replacement and required synchronization succeeded. |
| Either returns `nil, "too_large"` | Data exceeds 65,536 bytes; do not truncate or replace the existing save. |
| Either returns `nil, "io_error"` | A storage operation failed; report details to the console. For save, replacement has not occurred. |
| `save(data)` returns `nil, "durability_uncertain"` | Replacement occurred, but final synchronization failed. The new save may already be present; durability is not confirmed. |

A load failure must not be treated as an empty successful load. An unreadable
save is not equivalent to a missing save. On save, the distinction between
`io_error` and `durability_uncertain` prevents falsely claiming that the old
save remains unchanged after replacement has happened.

Games request saves explicitly at useful checkpoints or menus. Synchronization
may briefly stall gameplay; do not save every frame. `shutdown()` is an
additional opportunity, not the sole save strategy: it is skipped after Guru
errors and cannot cover power loss. Expected save failures should let a game
report the problem and continue.

## Storage location

The default save root is `$XDG_DATA_HOME/sugarbee/saves/`. If `XDG_DATA_HOME` is
unset or empty, use `$HOME/.local/share/sugarbee/saves/`. Relative XDG paths
are invalid and ignored, following the XDG specification. This is persistent
user data worth backing up, separate from cartridge files and caches.

A host command-line option, `--save-dir DIRECTORY`, overrides the root for Pi
deployment and isolated tests. It is not implemented yet. The host generates
one canonical UUID filename, `<uuid>.sav`, within the selected root. There is
no directory or filename selection from Lua.

An inaccessible selected location must produce a failure, not a silent fallback
to a different directory. Cartridge media may remain read-only while saves use
a separate writable location. Precise CLI option ordering and path handling
must be integrated with the existing parser during implementation review.

## Replacement and durability contract

Target the current local Linux filesystem foundation. For each save:

1. Validate the argument and byte limit before modifying storage.
2. Write a unique temporary file in the same directory as the destination.
3. Finish writing, synchronize the temporary file, and check errors before
   replacing the destination.
4. Atomically rename the completed temporary file over the destination.
5. Synchronize the containing directory before reporting success.

Account for directory creation too: a successful first save must include any
necessary synchronization of newly created directory entries, not merely the
final save directory. Implementation must check short writes and failures in
all relevant write, close, rename, and synchronization operations.

The three save outcomes are:

- **Failure before replacement:** SugarBee leaves the previous save untouched.
  Return `nil, "io_error"` for storage failures (or `too_large` for size rejection).
- **Success:** replacement and all required synchronization operations reported
  success. Return `true, nil`.
- **Failure after replacement:** the new save may already be visible, but its
  persistence has not been confirmed. Return `nil, "durability_uncertain"`;
  do not claim the old data remains or attempt an unproven rollback.

Atomic replacement avoids exposing a partially written replacement during
normal operation. It is distinct from crash durability. We will test interrupted
writes and recovery and document the observed guarantees before making claims
about power-loss protection. Successful synchronization relies on the
filesystem and storage device honoring their contracts; it cannot guarantee
survival of faulty media or every hardware failure.

No automatic backups are included. A game successfully saving incorrect data
replaces the previous save. Concurrent writers to the same UUID are outside the
initial supported scope. There is no runtime wall-clock timeout for storage I/O.

## Implementation checkpoints and acceptance checks

1. Implement bounded storage operations and test replacement/failure behavior.
2. Add Lua bindings, host directory selection, and a small save lab.
3. Validate on the development machine and Pi before merging the milestone.

Tests should cover:

- Missing saves, empty and binary strings, exact-limit data, oversized input,
  and oversized existing files; no silent truncation.
- Repeated writes, independent UUIDs, preserved UUIDs across repacking, and
  persistence across exit/relaunch and host restart.
- Unwritable locations and injected write/synchronization/replacement failures;
  verify the prior save is untouched before replacement.
- A failure after replacement: return `durability_uncertain`, not an unchanged-
  save claim or false success.
- Interrupted writes at defined stages, temporary-file handling, and first-save
  directory creation. Distinguish process interruption tests from actual
  power-loss testing.
- Real-host save failures that allow gameplay to continue and useful console
  diagnostics, plus ordinary lifecycle and cleanup regressions.

Temporary-file cleanup, unavailable home-directory handling, and filesystem
edge cases must be resolved during implementation without weakening the result
contract. The lab must not overwrite a user's real save directory.

## Deferred scope

Multiple slots, automatic table serialization, cloud synchronization, save
editing, automatic backups, and concurrent-writer coordination are not part of
this milestone. No general filesystem API is added. Actual OS shutdown remains
deployment work; a completed save is not permission to remove console power.

## Filesystem references

- [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir/latest/)
  defines user data locations and absolute environment paths.
- [Linux rename](https://man7.org/linux/man-pages/man2/rename.2.html) documents
  atomic destination replacement on the same filesystem.
- [Linux fsync](https://man7.org/linux/man-pages/man2/fsync.2.html) distinguishes
  synchronizing file contents from synchronizing the containing directory.

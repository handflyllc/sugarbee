# SugarBee saves — settled initial design

Status: initial API, storage policy, and durability contract agreed on 2026-10-29.
The Lua API is not implemented. Internal bounded C reading and synchronized
replacement in an existing directory are implemented; neither is connected to
the host or Lua. A separate one-level directory preparation helper is also
implemented; full-path creation and writer integration remain pending. This
document records the accepted design, not an available Lua API reference.

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

## First storage checkpoint: bounded reader

`sb_save_read` in `include/sb_save.h` accepts a host-selected directory and a
16-byte UUID. It reads the canonical UUID filename without creating files or
directories. Success returns caller-owned bytes and an explicit length, including
an allocated buffer for an empty save. Failures clear the outputs. The reader
returns distinct missing, oversized, I/O, and invalid-C-argument results.
Missing save directories are treated as no save; other directory errors are
storage failures. Save-file symlinks and non-regular files are rejected. The
host-selected root may itself resolve through a symlink.

The Linux/POSIX reader bounds allocation and reads to 65,537 bytes, using the
extra byte to detect oversize even if a file grows after its initial size check.
Concurrent modification is not a supported snapshot guarantee. Read operations
may update filesystem access metadata, but never write save contents.

The isolated `save_reader` CTest case covers empty and binary data, the exact
limit, oversize rejection, UUID selection, missing files/directories, invalid
arguments, read-only files, permission denial when run without root privileges,
and rejection of directory/FIFO/symlink save entries. A five-second CTest timeout
protects against accidental blocking. No real user save directory is used.

Run with `ctest --test-dir build -R '^save_reader$' --output-on-failure`.
Directory defaults, `--save-dir`, and Lua bindings remain unimplemented.
Pi validation of the storage checkpoints is pending.

## Second storage checkpoint: existing-directory writer

`sb_save_write` takes the selected directory, UUID, bytes, and length. The root
must already exist; a missing or inaccessible directory returns `SB_SAVE_IO_ERROR`
without creating it. The caller is responsible for provisioning the directory;
directory preparation is separate from the writer. NULL data is accepted only
for a zero-length save. Oversized input is rejected before filesystem changes.

The writer exclusively creates a temporary file in the opened directory with
owner read/write permissions (0600, further restricted by the process umask).
It handles short and interrupted writes, synchronizes and closes the completed
file, renames it over the UUID destination, then synchronizes the directory.
Interrupted synchronization is retried. Zero-progress writes fail instead of
spinning. Closing a descriptor is not retried after an error on Linux.

Existing non-regular save entries, including symlinks, are rejected. Replacement
produces a new file with the temporary file's permissions; it does not preserve
arbitrary metadata from a previous save. Concurrent directory changes and
writers are outside the supported scope.

Before replacement, storage failures return `SB_SAVE_IO_ERROR` and leave the
previous save untouched. After replacement, a directory-sync failure returns
`SB_SAVE_DURABILITY_UNCERTAIN`. A final directory-close failure also returns
that conservative result, even if synchronization already succeeded. The writer
never tries to roll back a replacement. Only successful file and directory
synchronization and descriptor closure return `SB_SAVE_OK`.

On early failure the writer attempts to remove only its own temporary file.
A failed removal is reported to stderr; the primary save outcome is unchanged.
Temporary names cannot be selected by the reader as UUID saves. Exclusive
creation tries up to 128 distinct candidate names; existing temporary entries
are never overwritten or automatically deleted. Exhaustion returns an I/O
error. Crash-leftover cleanup and interruption/power-loss testing remain later
work; these error-injection tests do not establish power-loss guarantees.

`save_writer` uses isolated temporary directories and the real reader to verify
first saves, replacement, empty/binary/exact-limit data, oversize rejection, and
UUID independence. Linker wrappers confined to the test executable inject
write, file-sync, file-close, rename, directory-sync, directory-close, and
cleanup failures. Tests verify unchanged old data before replacement, visible
new data with uncertain durability afterward, temporary cleanup (including a
failed cleanup), short/interrupted operations, collision handling, and rejected
non-regular destinations. Tests also check synchronization/close/rename ordering.
The production library has no fault-injection controls.

Run both storage cases through CTest:

```sh
ctest --test-dir build -R '^save_(reader|writer)$' --output-on-failure
```

The writer case has a 15-second external test timeout; storage I/O itself has
no runtime wall-clock bound. Full-path preparation, writer integration with
directory preparation, host/Lua integration, and Pi validation remain pending.

## Third storage checkpoint: one-level directory preparation

`sb_save_prepare_directory(parent, child)` prepares exactly one child beneath
an existing host-selected parent. The parent directory's own placement must
already be durable. This helper does not establish durability for its ancestors.
The child must be nonempty and cannot be `.`, `..`, an absolute path, or contain
`/`. Other ordinary Linux filename characters are allowed. Invalid arguments
return `SB_SAVE_INVALID_ARGUMENT` before filesystem operations.

Creation requests mode 0700 subject to the process umask. Existing permissions
are never changed. An existing child must be a real directory: file, FIFO, and
symlink entries are rejected, including symlinks pointing at directories. The
host-selected parent path may resolve through symlinks. Concurrent changes to
the directory tree remain outside the supported contract.

After creating or opening the child, the helper synchronizes the child, then
the parent, and closes both descriptors. It repeats synchronization for existing
directories too, because a previous attempt may have left creation incomplete
from a durability perspective. Interrupted synchronization is retried; close
errors are reported without retrying the descriptor close.

`SB_SAVE_OK` means preparation and required synchronization/closure completed.
`SB_SAVE_IO_ERROR` means preparation failed, but a created directory may remain.
The helper never rolls back by removing a directory, never edits save contents,
and never reports `SB_SAVE_DURABILITY_UNCERTAIN`: it performs no save replacement.
Callers must retry successfully before relying on preparation. The existing
writer still requires a prepared directory and does not invoke this helper.

The isolated `save_directory` test covers first creation, repeated calls,
permissions, invalid names, non-directory entries, parent aliases, and existing
save preservation. Test-only linker wrappers inject creation, child-open,
child/parent-sync, and child/parent-close failures. Each partial-creation case
is followed by a retry that must synchronize both directories. Interrupted
synchronization and operation ordering are also checked. The CTest case has a
15-second external timeout.

Run all storage checks with:

```sh
ctest --test-dir build -R '^save_(reader|writer|directory)$' --output-on-failure
```

These are desktop regression checks, not power-loss validation. Full-path
handling, writer integration, host/Lua integration, and Pi validation are pending.

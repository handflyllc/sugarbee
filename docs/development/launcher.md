# Cartridge launcher (experimental)

Run from the repository root after building:

```sh
./build/sugarbee
./build/sugarbee /path/to/game.vcart
./build/sugarbee --cartridges /path/to/vcarts
```

Quote paths containing spaces. Invalid command forms print usage and exit
before opening a window. No arguments opens an idle host, not a Guru error.
No default cartridge directory or configuration file is implemented.

## Discovery

Directory mode scans at startup and when requested with X/U, without
recursion. Only regular files ending in lowercase `.vcart` are candidates;
symbolic links are skipped. Each candidate must pass the existing
cartridge-header parser and a bounded, CRC-checked read of `main.lua`
through the asset reader. Discovery neither compiles nor executes Lua.
Empty `main.lua` files can pass; eligibility does not guarantee a working
game. Other assets are not individually decoded or CRC-checked during
discovery. See [asset limits](../api/assets.md).

At most 64 eligible cartridges are stored, in filesystem discovery order.
Additional eligible files are reported but omitted from the list. Full paths
must fit in 4096 bytes including the terminating byte. Inspection and path
errors have separate console diagnostics; eligible/rejected totals describe
regular candidates that reached validation, not every directory entry.
Directory-open and read failures are reported to the console. A partial list
may remain after a read failure. There is no persistent log file.

## Display and controls

Eight entries are visible at a time. Up/Down on any controller, or arrows/W/S,
moves one entry per press and stops at the list ends. A/J launches the selected
entry; B/K closes SugarBee. Empty lists ignore navigation and confirmation.
Titles and filenames identify entries, including duplicate titles. Long
filenames are shortened for display, and unsupported/control characters become
`?`; the stored launch paths are unchanged.

Exit Game returns to the list with the selection preserved. Each launch creates
a fresh Lua state. Launch presses are consumed, and held buttons remain hidden
from the cartridge until released. Direct-path mode instead offers A/J relaunch
of its startup cartridge. Without a startup path or directory, A/J does nothing.

Guru Meditation remains visible until B/K cleans up the failed session and
returns to the list or idle host. Failed sessions skip the Lua shutdown callback.
Holding the recovery button does not close the host; release and press again to
close it. The original diagnostic remains in the console. Recovery cannot
interrupt a Lua callback that never returns.

X/U rescans the directory, refreshes counts, and selects the first entry without launching it. Rescan works with empty lists and after directory errors. Holding the button does not repeat the scan.

The launcher displays its 64-cartridge capacity, additional eligible cartridges omitted from the list, and rejected-file count. Directory errors receive a separate message; detailed diagnostics remain in the console.

## Pending behavior

- Deterministic sorting.
- Automatic launch when exactly one cartridge is eligible.
- Configuration-file/menu directory settings, optional icons, and persistent
  logging are future discussion items, not implemented features.

See the [manual checklist](../../tests/launcher/README.md). The existing ten
CTest cases do not automate the complete launcher interaction flow.

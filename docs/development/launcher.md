# Cartridge launcher (experimental)

Run from the repository root after building:

```sh
./build/sugarbee
./build/sugarbee /path/to/game.vcart
./build/sugarbee --cartridges /path/to/vcarts
./build/sugarbee --console --cartridges /path/to/vcarts
```

Quote paths containing spaces. Invalid command forms print usage and exit
before opening a window. No arguments opens an idle host, not a Guru error.
No default cartridge directory or configuration file is implemented.

## Console mode: confirmed exit to system

`--console` must be the first option. It can be used alone, before a direct
cartridge path, or before `--cartridges DIRECTORY`. Hardware and display mode
never enable it automatically. It does not enable OS power-off.

At an idle console-mode host or launcher, Start/Enter opens an Exit to system
confirmation, with Cancel selected every time. Up/Down selects Cancel or Exit
to system; A/J activates the selected action. B/K or Start/Enter cancels.
Opening consumes simultaneous confirmation/navigation input. While open,
the panel blocks cartridge navigation, launch, and rescan. Inputs act on
press edges rather than repeating while held.

Confirming Exit to system stops the host loop, runs normal engine cleanup,
and exits successfully to the launching TTY or terminal. The OS keeps running.
This is not an indication that power can safely be removed, and it invokes
no OS shutdown command. Actual OS shutdown is deferred to console deployment.

B/K on the idle console-mode screen does nothing. Without `--console`, existing
B/K-to-close behavior remains and Start does nothing while idle. Start during
a game still opens the existing pause menu: exit the game before exiting to
the system. The game shutdown callback runs on Exit Game and is not called
again when leaving the idle host. Guru recovery remains B/K. Window-close/OS
quit events still close SugarBee normally, including during confirmation.

## Discovery

Directory mode scans at startup and when requested with X/U, without
recursion. Only regular files ending in lowercase `.vcart` are candidates;
symbolic links are skipped. Each candidate must pass the existing
cartridge-header parser and a bounded, CRC-checked read of `main.lua`
through the asset reader. Discovery neither compiles nor executes Lua.
Empty `main.lua` files can pass; eligibility does not guarantee a working
game. Other assets are not individually decoded or CRC-checked during
discovery. See [asset limits](../api/assets.md).

The first 64 eligible cartridges encountered are retained, then sorted by
case-sensitive title with filename as the tie-breaker. Sorting uses the original
strings, before display sanitization; uppercase ASCII letters precede lowercase.
Additional eligible files are reported but omitted from the list. When capacity
is exceeded, the retained subset still depends on filesystem discovery order;
it is not necessarily the alphabetically first 64 cartridges. Full paths
must fit in 4096 bytes including the terminating byte. Inspection and path
errors have separate console diagnostics; eligible/rejected totals describe
regular candidates that reached validation, not every directory entry.
Directory-open and read failures are reported to the console. A partial list
may remain after a read failure. There is no persistent log file.

## Startup launch policy

A startup scan automatically launches the sole eligible cartridge only when
there are no omitted eligible entries and no directory-scan errors. Rejected
files do not prevent that launch. Zero or multiple eligible cartridges leave
the launcher visible; scan errors also prevent automatic launch.

Automatic launch happens only at startup. Exit Game, Guru recovery, and X/U
rescan return to or remain in the launcher, even with exactly one entry. A/J
can launch that entry manually. A sole cartridge that fails during execution
uses the normal Guru recovery path, without an automatic retry loop.

## Display and controls

Eight entries are visible at a time. Up/Down on any controller, or arrows/W/S,
moves one entry per press and stops at the list ends. A/J launches the selected
entry; B/K closes SugarBee in desktop mode. Empty lists ignore navigation and confirmation.
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
close it. The original diagnostic remains in the console. Instruction limits
catch ordinary runaway main-thread Lua execution in the supported lifecycle
stages. Recovery still cannot interrupt blocked native calls or execution
outside those limits; see [lifecycle limits](../api/timing.md).

X/U rescans the directory, refreshes counts, and selects the first entry without launching it. Rescan works with empty lists and after directory errors. Holding the button does not repeat the scan.

The launcher displays its 64-cartridge capacity, additional eligible cartridges omitted from the list, and rejected-file count. Directory errors receive a separate message; detailed diagnostics remain in the console.

## Future discussion

- Configuration-file/menu directory settings, optional icons, and persistent
  logging are future discussion items, not implemented features.

See the [manual checklist](../../tests/launcher/README.md). The automated suite
includes twenty native tests and, when Lua 5.4 is available, eight cartridge smoke
tests using a stand-in API. It does not automate the launcher interaction flow.

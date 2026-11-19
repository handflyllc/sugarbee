# Persistent packaging identity

Completed checkpoint at `73ee6a3`; desktop and Pi accepted 2026-11-19.

A game's UUID is its save identity. Changing title, assets or package filename
must not change that identity. The runtime still reads the existing 64-byte
cartridge header; no format or save API changes are involved.

## New games

Initialize once, then commit the generated `sugarbee.uuid` with the game source:

```sh
./build/sbee-pack -i path/to/cart --init-identity
./build/sbee-pack -i path/to/cart -o build/game.vcart -t "My Game"
```

Normal packaging reads `sugarbee.uuid` automatically and never writes source
metadata. Initialization refuses to overwrite any existing identity file.
An independent game made by copying an existing project needs its own identity;
keep the original identity for updates to that game.

## Existing games

Adopt the UUID used by the existing released cartridge or packaging script:

```sh
./build/sbee-pack -i path/to/cart --init-identity --uuid 123e4567-e89b-42d3-a456-426614174000
```

The example UUID is illustrative: use **your game's existing value**. Generating
a fresh identity does not migrate saves. Old saves remain on disk, associated
with the original UUID. Initialization cannot infer which old build is yours.

## Contract and compatibility

- Root `sugarbee.uuid` contains exactly a canonical UUIDv4 (36 ASCII bytes),
  optionally followed by one LF newline. Hexadecimal letters may be either case;
  generated files use lowercase and a newline. CRLF, extra data and invalid UUIDs
  fail validation. Reads are bounded to 38 bytes.
- The file must be a regular file; directories, symlinks, unreadable and malformed
  metadata fail packaging. They never trigger fallback generation.
- `--uuid` may match the recorded identity; a mismatch fails before output is
  opened. Comparison uses UUID bytes, not letter case.
- Without metadata, existing `--uuid` commands continue to work. Without either,
  legacy packaging generates a UUID and warns. Retain the printed value and adopt
  it before repacking a game whose saves matter.
- Initialization exclusively creates the file, refusing replacement, and accepts
  `-i`, `--init-identity`, and optional `--uuid`; no output or title is needed.
- Root `sugarbee.uuid` is excluded from the ZIP. A nested file of that name remains
  an ordinary game asset. `console.json` retains its canvas-only contract.
- This is an identity workflow, not a general manifest, inclusion-rule system,
  save migration feature or runtime capability declaration.

## Validation

`packager_identity` covers adoption, generated identities, repeat builds, binary
header identity, matching/conflicting CLI UUIDs, malformed files, directories,
symlinks, output preservation on identity rejection, root-only metadata exclusion,
and legacy behavior. The existing save tests cover UUID-based runtime storage.

On 2026-11-19, the desktop build and full serial suite passed: **178/178**.
The identity regression also passed AddressSanitizer and UndefinedBehaviorSanitizer
with `ASAN_OPTIONS=detect_leaks=0`; leak detection was not claimed. The user confirmed all desktop manual checks passed on 2026-11-19:
saved counter survived repacking without --uuid, Pause/resume and normal exit
worked, and repeated initialization refused replacement. On 2026-11-19, the user confirmed Pi validation against `73ee6a3`: **178/178**
tests passed, the saved counter survived repacking without --uuid, Pause/resume
and normal exit worked, and repeated initialization refused replacement.

Manual desktop and Pi check (use the same commands on each machine):

```sh
mkdir -p build/identity-lab/cart
cp examples/save-lab/main.lua build/identity-lab/cart/main.lua
./build/sbee-pack -i build/identity-lab/cart --init-identity
./build/sbee-pack -i build/identity-lab/cart -o build/identity-lab/game.vcart -t "Identity A"
./build/sugarbee --save-dir build/identity-lab/saves build/identity-lab/game.vcart
```

1. Press Right three times, then J to save 3. Exit to the host/system normally.
2. Repack and launch using the same identity without specifying `--uuid`:

   ```sh
   ./build/sbee-pack -i build/identity-lab/cart -o build/identity-lab/game.vcart -t "Identity B"
   ./build/sugarbee --save-dir build/identity-lab/saves build/identity-lab/game.vcart
   ```

3. Confirm the saved counter loads as 3. Check Pause/resume and normal exit.
4. Run initialization again: it must refuse, preserving `sugarbee.uuid`.

For repeat validation, skip initialization if the file already exists. These
commands use isolated test saves under `build/identity-lab`, not normal saves.

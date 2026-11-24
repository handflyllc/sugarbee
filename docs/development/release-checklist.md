# SugarBee RC2 release-readiness checklist

Status: preparation, not acceptance or publication. Reviewed 2026-11-23 against
`41d43d0`; accepted feature baseline `1fb9542`. No new runtime features are in
scope. The latest implementation checkpoint `953a202` passed 204 serial tests
and user manual checks on desktop and Pi. Those results are development evidence,
not fresh Release or final distribution validation.

The [RC1 acceptance record](release-checklist-rc1.md) is preserved separately.
See [documentation findings](rc2-documentation-review.md),
[release notes](../../RELEASE_NOTES.md) and [packaging](../release/PACKAGING.md).

| Gate | RC2 status |
| --- | --- |
| Documentation reconciliation | Review recorded; packaging-specific instructions must follow the final example selection |
| Runtime scope | Current engine; discuss confirmed correctness/package blockers before expanding scope |
| Packaging refresh and example selection | Pending; existing scripts stage the RC1 six-cartridge set |
| Fresh desktop Release suite and manual checks | Pending |
| Fresh Pi ARM64 Release suite and manual checks | Pending |
| Independently extracted source build and full suite | Pending |
| Extracted x86_64 and ARM64 binary acceptance | Pending |
| Final notices, build identity and checksums | Pending |
| Matching commit/tag/artifacts and publication | Pending; intended tag `v0.1.0-rc2` |

## Fresh native Release validation

On each machine, record the commit, platform, configuration, test count and manual
results. Use a fresh build directory and ensure the Lua 5.4 interpreter is
installed; missing optional test prerequisites must not silently reduce coverage.

```sh
git rev-parse HEAD
cmake -S . -B build-rc2-release -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DSUGARBEE_BUILD_HOST=ON -DSUGARBEE_BUILD_PACKER=ON
cmake --build build-rc2-release -j4
ctest --test-dir build-rc2-release --output-on-failure
```

Expected current full suite: 204 tests. Preserve failing logs before rerunning.
The plain `ctest` command assumes `CTEST_PARALLEL_LEVEL` is unset or `1`.
CTest is serial on both targets; parallel compilation is permitted. Review any
warnings. SDL dummy-driver checks do not establish visible rendering, audible
playback or physical-controller behavior.

## Packaging checkpoint to settle next

Refresh `tools/package-release.sh`, `tests/release/prepare-fixtures.sh` and their
instructions together. Keep the existing representative coverage and choose a
bounded addition for native music and newer graphics/text capabilities. Candidates
include song-change, canvas/rotation demonstrations and panes-demo; this is not
a decision to ship every example. Record tested engine requirements with the
selected cartridges. Keep source identities stable across builds.

The existing header minimum-runtime value is `1.0.0` in RC1 and current main.
It cannot distinguish RC2-only APIs; do not claim machine enforcement of the
release requirement. See [version semantics](../specifications/cartridge-format.md).

Use source plus native Linux x86_64 and ARM64 archives with system shared-library
dependencies. No bootable OS image or installer is included. The packaging guard
requires a clean checkout including untracked files; preserve transfer notes
outside that checkout or package from a separate clean checkout.

## Manual and extracted-package acceptance

Finalize exact commands with the packaging refresh. Cover:

- Launcher, pause/resume, Exit Game, relaunch, Guru recovery and normal host exit.
- Pong/Pac-Man baseline behavior and simultaneous physical controllers where applicable.
- Selected graphics/canvas/text demonstrations, including Enter submission and
  Escape/controller Start pausing during text entry.
- Synth listening across the supported range, native music transport/song changes,
  WAV overlap/looping, and silence after exit or appropriate relaunch.
- Save/relaunch with stable cartridge identity and intentional deletion using
  disposable validation saves.

On each target, extract its binary archive outside the development checkout,
verify supplied checksums and resolved shared libraries, launch every shipped
cartridge and repack an included source example using the shipped packer.
Separately extract the source archive and perform a fresh Release build/full
serial suite. Verify staged notices, documented dependencies and complete source
contents, including vendored libraries. Record archive names and BUILD-INFO commits.

After acceptance, reconcile final release notes/checklist and ensure the tag,
source archive and both native archives identify the same accepted commit.
Generate/verify final checksums after artifact creation; user performs credentialed
pushes and publication. No release artifact has been accepted by this document.

# SugarBee 0.1 release-readiness checklist

Status: in progress, 2026-11-06. Runtime baseline: `b304a00`. Readiness preparation was committed/pushed as `f99583a` on
`docs/release-readiness` to make the same helper available on the Pi. Subsequent
acceptance records remain uncommitted.
Finalization is authorized; the release tag and artifact BUILD-INFO establish
the published identity. Accepted scope is `v0.1.0-rc1`: source plus Linux x86_64 and ARM64 binary
archives, with provisional APIs and documented system dependencies.

| Item | Evidence / remaining work |
| --- | --- |
| Current documentation | Reconciled with baseline; local links and whitespace checked |
| Coastline fresh Release build | `build-rc-release`, Release, full 89 tests pass, no compiler warnings (2026-11-06) |
| Coastline Release manual acceptance | User-reported 2026-11-06: all six check groups pass; Exit to system 0 |
| Pi fresh Release build | User-confirmed `f99583a`, all 89 Release tests pass (2026-11-06) |
| Pi Release manual acceptance | User-reported complete on 2026-11-06, including two controllers simultaneously |
| Two physical controllers | Both work on coastline; simultaneous operation accepted on Pi |
| Release contents and provisional API policy | Accepted: source plus two native Linux archives, provisional API; see release notes |
| Binary package tooling | Native build, examples, notices, build identity and checksums implemented; preview guard verified |
| x86_64 extracted package | Preview automated checks pass; user confirmed all six extracted cartridges and exit 0 (2026-11-06) |
| ARM64 extracted package | User-reported at `409c505` (2026-11-06): libraries resolve, all cartridges pass, repacked Pong launches/pauses/restarts, synth listening and applicable pause/resume pass |
| Source distribution contents | `git archive` of `f99583a` inspected; fresh extracted Release build has no warnings, 89 tests pass, acceptance fixture helper succeeds; recheck final tagged-tree contents after documentation commits |
| Final commit / tag / publication | Authorized after acceptance; user performs Git writes/pushes, builds from `v0.1.0-rc1`, verifies final archives before publication |

The existing Debug and hardware results remain valid historical evidence.
They do not replace acceptance of the Release executable. This is a finite
baseline check, not a new feature phase or a full hostile-cartridge audit.

## Clean Release build

On each host, from the repository root:

```sh
git rev-parse --short HEAD
cmake -S . -B build-rc-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-rc-release -j4
ctest --test-dir build-rc-release --output-on-failure
```

Use a new build directory for the initial check. Coastline's fresh build has
already completed. If `build-rc-release` exists on the Pi, choose another unused
directory and substitute it throughout; do not delete prior work or saves.
Install the standalone `lua5.4` interpreter to enable all cartridge smoke tests.
Expect 89 tests for this baseline, rather than accepting a smaller skipped suite.

## Prepare and launch the acceptance set

```sh
bash tests/release/prepare-fixtures.sh ./build-rc-release
./build-rc-release/sugarbee --console --save-dir ./build-rc-release/acceptance-saves --cartridges ./build-rc-release/acceptance
echo $?
```

The script uses only binaries from the selected build. It packages Pong,
Pac-Man, sprite-sheet, Save Lab, basic/musical Synth Labs, WAV Lab, and a deliberate
runaway-update cartridge for Guru recovery. No test source selector is edited.
Generated packages remain in the build tree; rerunning preserves saves. Save
Lab retains its established UUID. These are test saves, not production backups.

The helper is available in preparation commit `f99583a`. Runtime source is
unchanged from `b304a00`; record the actual tested commit and any local changes.
The Pi remains a receive-only validation target.

## Compact manual acceptance — coastline and Pi

1. **Launcher and graphics:** navigate the list, launch the sprite-sheet demo,
   check movement, transparency, animation and flipped sprites; pause/resume,
   exit to launcher, rescan with X/U, and launch another cartridge.
2. **Gameplay and input:** play Pong and Pac-Man with keyboard and an available
   controller; verify pause, resume, exit and relaunch. No stuck inputs or Gurus.
3. **Saves:** in Save Lab change and save the counter, exit/relaunch and confirm
   it reloads. Exit the host completely, relaunch with the same save directory
   and confirm persistence. Do not infer power-loss durability from this check.
4. **Recovery:** launch Execution Limit, confirm a Guru, dismiss with B/K, then
   launch Pong and verify normal play. The runaway cartridge is intentionally bad.
5. **Audio:** exercise basic Synth Lab controls and musical playback; in WAV Lab
   check both one-shots, sustained looping, overlap, pan/volume/stop, pause/resume
   and silent relaunch. A loop should continue beyond ten seconds without a
   recurring seam click or gap. Use the individual lab READMEs for key mappings.
6. **Exit:** use Exit to system and confirm status 0. Relaunch the host once to
   confirm normal startup. Record device/output and anything unexpected.

## Two-controller coverage

With two physical controllers connected, move both Pong paddles independently
and simultaneously. Check each paddle's bounds and restart after a win using
B on each controller in separate runs. Disconnect/reconnect a controller and
check recovery without a crash or stuck movement; verify the assigned players
against the discovery-order policy in the input API. Record device models and
host. If hardware is unavailable, keep the gap explicit and agree whether it
is acceptable for the RC; do not silently mark it passed.

## Finalization

Once acceptance and release contents are agreed, commit the readiness changes
on a focused branch. Inspect a source archive from that exact commit: include
source, tests, tools, examples, docs, LICENSE and third-party notices; exclude
local builds, saves, credentials and ignored handoff material. Verify the archive
build instructions in a fresh extraction before tagging. If runtime source
changes during readiness, repeat affected checks and refresh acceptance evidence.
Tag the accepted commit only after remaining gates are resolved; publish/push
through the user's normal terminal workflow. Source and both native binary archives are accepted scope; no installer or
appliance image. See [packaging](../release/PACKAGING.md) for artifact identity
and extracted-package acceptance.

The basic and musical Synth Labs use the default player index 0 (Player 1).
Player 2 not operating their sound controls is expected, not an input failure.

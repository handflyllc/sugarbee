# RC2 source and native binary packaging

The selected set is the six RC1 cartridges plus song-change, canvas-demo,
rotation-demo and panes-demo. Packaging refresh is implemented; fresh Release,
manual and extracted-archive acceptance remain separate pending gates in the
[current checklist](../development/release-checklist.md).

The final-package clean-tree guard also rejects untracked files; preserve local
transfer notes outside the release checkout or use a clean checkout.

Accepted scope: a source archive plus native Linux x86_64 and ARM64 binary
archives. No installer, bundled shared libraries or appliance image. Use a
native Release build on coastline for x86_64 and on the validated Pi for ARM64.
The source/API remains provisional; publication is a separate final action.

## Packaging commands

From the repository root on either host:

```sh
bash tools/package-release.sh build-rc2-release 0.1.0-rc2
```

This requires a clean committed checkout. It configures Release mode, builds
`sugarbee`, `sbee-pack` and the WAV generator, and creates a new `package.*`
directory beneath the chosen build directory. Existing packages and saves are
not removed. It does not run CTest or claim hardware acceptance automatically.
Use the full Release acceptance procedure before release.

For a local packaging trial while documentation/tooling changes are uncommitted:

```sh
bash tools/package-release.sh build-rc2-release 0.1.0-rc2 --preview
```

Preview binary archives have `-preview` in the name and a warning in BUILD-INFO.
They include current working-tree content and are not release artifacts. Preview
mode does not generate a misleading source archive from an older HEAD.

Final output contains:

- `sugarbee-0.1.0-rc2-linux-x86_64.tar.gz` or `...-linux-arm64.tar.gz`.
- `sugarbee-0.1.0-rc2-source.tar.gz`, made with `git archive HEAD`.
- `SHA256SUMS` for those archives.

Each binary package includes the host and packager, ten playable cartridges,
their source examples, API/documentation references, LICENSE and attribution,
BUILD-INFO (commit, architecture, OS, libc and resolved libraries), and internal
binary/cartridge checksums. No local handoff, save data or shared libraries are
copied. Packaged source examples contain `sugarbee.uuid` with their fixed distribution
identities, so the included packer preserves identity on rebuild. Repository
example sources are not modified. WAV assets are generated
from the tracked source. The deliberate Guru acceptance fixture is not shipped
as a playable release example.

## Extraction check

Copy/extract the archive into a directory outside the source checkout. Verify
SHA256SUMS before extraction and CONTENTS.sha256 afterward. Follow the packaged
README for library resolution, play/listening, both controllers, saves, relaunch
and Exit to system. Run the packaged `sbee-pack`, not the development executable.
Record the archive filename, BUILD-INFO commit, OS and actual checks performed.

## RC2 preview automation — 2026-11-23

On Debian x86_64, the working changes based on `8729686` produced preview
`build-rc2-release/package.CqJ7Mg/sugarbee-0.1.0-rc2-linux-x86_64-preview.tar.gz`.
Archive and internal checksums passed after independent extraction outside the
checkout; both executables resolved their shared libraries. All ten cartridges
passed ZIP CRC checks. Each included source was repacked with the shipped packer
without `--uuid`, and all ten retained their original cartridge identity.
All twenty original/repacked cartridges launched with dummy SDL video/audio,
remained running for one second, and exited 0 on SIGTERM without Guru diagnostics.
Vendored yyjson license and font source notice were present. The acceptance
fixture helper also produced twelve CRC-valid cartridges (ten plus Pac-Man and
the deliberate Guru fixture). Shell syntax and local Markdown links passed.

These are automated packaging checks, not listening, rendering, controller,
clean-machine dependency installation, Pi or final-release acceptance. No source
archive is generated in preview mode. Final packages must be regenerated from
the accepted clean commit, including subsequent documentation updates.

The fresh optimized build emitted four `-Wformat-truncation` warnings at
`src/sb_music_load.c:88` and `:93`, in existing bounded diagnostic formatting.
Inspection found paths built from fixed schema names and bounded track/event
indices, well below the 128-byte destination; no runtime defect was established.
These warnings are recorded rather than claiming a warning-free Release build;
no music-loader change is included in this packaging checkpoint.

The user confirmed all ten desktop acceptance checks passed on 2026-11-23
for this extracted preview. This accepts the desktop packaging checkpoint based
on `8729686` plus the working changes; it does not establish ARM64 acceptance
or final artifact/tag identity. The subsequent harness repair changes tests only.

### Fresh Release suite: initial failure and harness repair

The full serial Release suite completed with **201/204 passing** (175.92 seconds).
Failures: `music_assets`, `music_load`, `music_assets_sdl`, all in allocation
accounting/failure-injection assertions. Inspection of the optimized loader object
shows the compiler replaced `malloc` plus zeroing with `calloc`; these three test
harnesses wrap `malloc` and `free`, but not `calloc`. That bypasses their allocation
tracking while frees remain tracked. This is strong evidence of an optimized-build
test instrumentation defect, not evidence of an engine leak. The user subsequently authorized a focused harness repair. All three tests now
wrap both allocation entry points, retaining failure injection and cleanup
assertions. The SDL ownership test begins tracing after host-session creation
so its counters continue to measure cartridge assets. No production engine code
or optimization flags changed. Focused Debug and Release runs both pass 3/3;
full serial Release revalidation passed **204/204**. The original three failures
are resolved by test-only instrumentation changes. The user confirmed all ten desktop acceptance checks passed on 2026-11-23.
Pi Release validation remains pending.

Logs are preserved under `build-rc2-release/validation-8729686/`, including the
original failing full-suite log, build warnings, fixture and extraction checks.
The original failure log is retained separately from repair-validation logs. Pi and final artifact validation remain pending.

## Historical RC1 evidence (not RC2 acceptance)

Coastline preview trial, 2026-11-06: x86_64 archive checksums and internal checksums
passed; all runtime libraries resolved. The packaged packager rebuilt Pong from
included source. All six shipped cartridges and repacked Pong loaded from an
independent temporary extraction with dummy SDL drivers and exited 0 on an SDL
termination signal. This verifies package execution and cleanup, not physical
input, rendered appearance, audible playback or clean-machine dependency setup.
The user subsequently confirmed all six extracted x86_64 preview cartridges
passed manual checks and the host returned status 0 (2026-11-06). The tested
preview was generated under `build-rc-release/package.sfkTI3`, based on
`f99583a` plus the uncommitted packaging/readiness changes. Final artifacts must
be regenerated from the accepted clean commit.

ARM64 extracted-package acceptance, user-reported 2026-11-06 at `409c505`:
all libraries resolve and all cartridge checks pass. Repacked Pong launches,
pauses and restarts. Synth Labs sound correct and resume where supported after
pause. This is Pi package acceptance, not a claim about other ARM64 distributions.

## Final artifact identity

Both native packages must report the same accepted source commit. A final tag
must identify that commit. If documentation is committed after package testing,
regenerate final packages so their BUILD-INFO and bundled notes match the tag;
inspect changed contents and perform the package check appropriate to those
changes. Runtime changes require renewed relevant tests and hardware acceptance.

Collect the source and both native archives into one release directory. Generate
one SHA256SUMS covering those three files and verify it there. Keep build reports
and acceptance evidence with the release record. Publish only the final artifacts,
not preview archives, intermediate staging folders or test saves. This workflow
does not itself tag, push or publish anything.

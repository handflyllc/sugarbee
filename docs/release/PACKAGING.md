# RC1 source and native binary packaging

Accepted scope: a source archive plus native Linux x86_64 and ARM64 binary
archives. No installer, bundled shared libraries or appliance image. Use a
native Release build on coastline for x86_64 and on the validated Pi for ARM64.
The source/API remains provisional; publication is a separate final action.

## Packaging commands

From the repository root on either host:

```sh
bash tools/package-release.sh build-rc-release 0.1.0-rc1
```

This requires a clean committed checkout. It configures Release mode, builds
`sugarbee`, `sbee-pack` and the WAV generator, and creates a new `package.*`
directory beneath the chosen build directory. Existing packages and saves are
not removed. It does not run CTest or claim hardware acceptance automatically.
Use the full Release acceptance procedure before release.

For a local packaging trial while documentation/tooling changes are uncommitted:

```sh
bash tools/package-release.sh build-rc-release 0.1.0-rc1 --preview
```

Preview binary archives have `-preview` in the name and a warning in BUILD-INFO.
They include current working-tree content and are not release artifacts. Preview
mode does not generate a misleading source archive from an older HEAD.

Final output contains:

- `sugarbee-0.1.0-rc1-linux-x86_64.tar.gz` or `...-linux-arm64.tar.gz`.
- `sugarbee-0.1.0-rc1-source.tar.gz`, made with `git archive HEAD`.
- `SHA256SUMS` for those archives.

Each binary package includes the host and packager, six playable cartridges,
their source examples, API/documentation references, LICENSE and attribution,
BUILD-INFO (commit, architecture, OS, libc and resolved libraries), and internal
binary/cartridge checksums. No local handoff, save data or shared libraries are
copied. Source examples retain fixed package identities. WAV assets are generated
from the tracked source. The deliberate Guru acceptance fixture is not shipped
as a playable release example.

## Extraction check

Copy/extract the archive into a directory outside the source checkout. Verify
SHA256SUMS before extraction and CONTENTS.sha256 afterward. Follow the packaged
README for library resolution, play/listening, both controllers, saves, relaunch
and Exit to system. Run the packaged `sbee-pack`, not the development executable.
Record the archive filename, BUILD-INFO commit, OS and actual checks performed.

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
be regenerated from the accepted clean commit. ARM64 packaging and extracted
acceptance remain pending.

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

# SugarBee Linux binary package

This package is an experimental RC candidate, not a bootable console image.
A filename containing `preview` is a local packaging trial, not a release.
See BUILD-INFO.txt for the exact commit, architecture, build OS and resolved
libraries. The API remains provisional; see RELEASE_NOTES.md.

## Requirements

Use the archive for your CPU: x86_64 for a compatible PC, ARM64 for 64-bit
Raspberry Pi Linux. These are dynamically linked native binaries, not portable
executables for every Linux distribution. Compatibility is established only on
the environments recorded in the release acceptance results. No shared libraries
are bundled. Install runtime packages through your distribution's package manager.

On Debian 12 / a compatible Raspberry Pi OS Debian base:

```sh
sudo apt install libsdl2-2.0-0 liblua5.4-0 libminizip1 libpng16-16 zlib1g zip
```

`zip` is needed by `sbee-pack`; the game host does not invoke it. Package names
can differ on other distribution versions. No compiler, CMake or standalone
Lua interpreter is required to run this binary package. A working SDL video,
audio and input environment is required. Missing audio permits silent gameplay.

## Verify and run

Verify the download against its accompanying SHA256SUMS, then extract it into
a writable location. From inside the extracted package:

```sh
sha256sum -c CONTENTS.sha256
ldd bin/sugarbee
ldd bin/sbee-pack
./bin/sugarbee --console --cartridges ./cartridges
echo $?
```

No dependency should report `not found`. Do not run SugarBee as root.
Navigate with arrows/WASD or the controller D-pad, A/J selects, Start/Enter opens
the host menu. Exit Game returns to the launcher; Exit to system closes the host
with status 0 and does not power off the OS. Labs use Player 1 unless documented
otherwise; Pong supports two controllers. Cartridge-specific controls are in
`examples/<name>/README.md` where present. Sprite-sheet movement uses the D-pad
or arrow/WASD keys. Samples start silently until triggered.

Saves normally use `$XDG_DATA_HOME/sugarbee/saves` or
`$HOME/.local/share/sugarbee/saves`. For isolated acceptance testing, add
`--save-dir ./acceptance-saves` before `--cartridges`; keep that directory if you
want to preserve test saves. Preserve game UUIDs when repackaging.

Package an included example with the shipped tool:

```sh
./bin/sbee-pack -i examples/pong-lab -o ./pong-test.vcart -t "Pong test"
```

For your own games, record the generated UUID and reuse it with `--uuid` across
updates. Original source examples and API references are included for reference;
repository-relative build/test links in those references require the full source
release. This binary package does not include the development test toolchain.

## Extracted-package acceptance

Use an extraction outside the development checkout on each target host:

- Verify checksums and resolved libraries; package and launch the Pong test above.
- Launch each included cartridge: Pong, basic/musical Synth Labs, Save Lab,
  sprite-sheet demo and WAV Lab. Check graphics, controls and audio.
- In WAV Lab test sustained looping, overlap, stop and pause/resume; exit while
  sounding and relaunch silently. Check both controllers in Pong.
- Save/relaunch Save Lab with the same save directory and confirm persistence.
- Exit the host normally and confirm status 0.

Dummy-driver automated execution is not listening, display, physical-controller
or clean-machine dependency-installation acceptance. Report the archive name,
architecture, BUILD-INFO commit and checks performed.

## Attribution

Original SugarBee code uses the included zlib LICENSE. The embedded font's
upstream notice and source are retained under `third_party/font8x8/`. The musical
lab records its score source and adaptation in its README; no recording is bundled.
WAV Lab contains original generated samples. System libraries retain their own
licenses and are installed separately. See the source release for build scripts,
tests and complete development documentation.

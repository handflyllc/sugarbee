# SugarBee 0.1.0-rc2 — draft release notes

**Unreleased.** RC2 freezes the engine feature baseline at `1fb9542`; preparation
may update documentation and packaging. No RC2 tag, final artifact identity,
fresh Release-build acceptance or package acceptance is claimed yet. See the
[RC2 checklist](docs/development/release-checklist.md). RC1 notes are retained below.

## Changes since RC1

- Native music JSON loading, sample-frame sequencing, loop eligibility, explicit
  voice reservations, transport/gain and coordinated host pause/cleanup.
- A0–C8 pitched synthesis (27.5–4186.01 Hz), corrected pulse edges, and direct
  phase/envelope-preserving frequency control; no scheduled pitch glide.
- Clip rectangles, camera offset, sprite tint/opacity, rotation with pivots,
  lines, circles/disks and filled triangles; rectangle/text opacity.
- Fixed logical canvas declarations and width/height queries; text measurement,
  cartridge-local wrapping and line-editing helpers, explicit ASCII text entry.
- Cartridge-local `require`, bounded general-purpose `read_file`, persistent
  source UUID initialization, explicit save deletion and an opt-in `--stats` overlay.
- Tested examples including song changes and a three-pane command interface.

## Compatibility and scope

The API remains provisional; this is not a stable ABI. Cartridge format v1 and
existing save UUID semantics remain. New APIs/assets may require RC2 even though
RC1 can read the container. The existing header runtime check uses 1.0.0 in both
versions and cannot distinguish RC1/RC2 capabilities; see the
[version distinction](docs/specifications/cartridge-format.md#release-labels-versus-the-existing-runtime-check).
Document the required release alongside each game. Do not treat header acceptance
as API compatibility or change a game's UUID simply because it is repackaged.

Planned distribution: source plus native Linux x86_64/ARM64 archives, dynamically
linked to system libraries. No bundled OS/SugarHive image or new runtime feature
is included in this release-preparation scope. Mouse/analog input, custom-font
APIs, additional text-edit keys, external scenarios and audio extensions remain
separate future work. Run trusted cartridges: resource limits are not a hostile
sandbox, and synchronized saves do not guarantee power-loss durability.

## Evidence and remaining gates

The latest implementation checkpoint `953a202` passed 204 serial tests and manual
checks on desktop and Pi on 2026-11-21. These are development-checkpoint results;
fresh Release builds, extracted distributions and final artifact identity are
still pending. Historical Pi lockups remain unexplained; CTest stays serial.
The module fixture setup allowance was raised to 20 seconds and passed Pi
recheck; the original delay was not diagnosed as storage failure.

---

# SugarBee 0.1.0-rc1 — release notes

These notes describe the published RC1 tag `v0.1.0-rc1` (`cca7c62`), not current
main. Native music playback was added after RC1; see the [roadmap](ROADMAP.md).

Release candidate for the experimental 0.1 baseline: source plus Linux x86_64
and ARM64 binaries. Package acceptance is complete. Official artifacts must be
built from tag `v0.1.0-rc1`; binary BUILD-INFO files identify the exact commit.
See the [RC1 readiness checklist](docs/development/release-checklist-rc1.md) for evidence
and the final artifact verification procedure.

SugarBee is an experimental 2D-first virtual console runtime for Debian x86_64
and Raspberry Pi 4 ARM64 Linux. Cartridges contain Lua 5.4 code and assets in
versioned `.vcart` packages. Native host binaries must be built for each platform.

## Included baseline

- Bounded cartridge/asset loading, PNG sprites, sprite-sheet crop/scale/flip,
  basic drawing and text.
- Controller and keyboard input, fixed 60 Hz simulation, launcher and pause menu.
- Restricted Lua libraries, ordinary runaway-loop instruction budgets and Guru recovery.
- Explicit UUID-based saves, up to 64 KiB per game, with synchronized replacement.
- Nine synth voices with triangle/saw/pulse/noise, ADSR, pan/volume, waveform
  transitions and a simple low-pass filter. Pitched notes span 110–880 Hz.
- Eight independent WAV playbacks with shared assets, one-shots and whole-sound
  looping, pan/volume/stop, and coordinated pause/exit cleanup.
- Source examples, diagnostic tools, automated tests and API/build documentation.

## Compatibility and limits

This is a provisional 0.1 API, not a stable ABI or a promise of compatibility
with future versions. Preserve cartridge UUIDs across repackaging for save
identity. Current `.vcart` format is version 1; use the documented SugarBee APIs.

Run trusted cartridges. Library restrictions, resource bounds and instruction
budgets do not form a hostile-code sandbox. Native blocking, compilation,
memory exhaustion and repeatedly caught errors are outside general recovery
guarantees. Saves are explicit and do not promise hardware power-loss durability.

No bootable appliance image, OS shutdown, Windows packager, analog controls,
configurable logical resolution, WAV streaming, filter resonance or automatic
audio-device reconnection is included. Audio loops require suitably authored
sample endpoints. Detailed API limits remain in the reference documentation.

## Build and validation

Follow [dependencies](docs/development/dependencies.md) and
[build instructions](docs/development/building.md). Include the standalone
Lua 5.4 interpreter for the full 89-test suite.

The runtime baseline is `b304a00`. Coastline's fresh Release build passed all
89 tests without compiler warnings on 2026-11-06. The user confirmed all
coastline manual acceptance checks, both controllers working, and exit status 0.
The user reported Pi Release acceptance complete on 2026-11-06, including
simultaneous two-controller operation. The tested Pi commit is `f99583a`, with all 89 Release tests passing. Release finalization was authorized after package acceptance. The release tag
identifies the final commit; runtime source is unchanged from the tested baseline.

A source archive of `f99583a` was extracted independently of the checkout:
its clean Release build completed without warnings, all 89 tests passed, and
the acceptance-fixture helper succeeded. For reproduction, generate artifacts from the final RC1 tag and verify their
contents and checksums using the packaging procedure.

## Distribution and attribution

Distribution: a source archive plus native Linux x86_64 and ARM64 binary
archives, with LICENSE, third-party notices, examples and build/run instructions
retained. Binaries use system libraries; no libraries or installer are bundled.
SugarBee's original code uses the zlib license. Font attribution/provenance is
in `third_party/font8x8/`; the musical lab records its score source in its README.
WAV Lab samples are generated from original test code. System dependencies are
installed separately and retain their own licenses. Binary package preparation and acceptance are described in
[packaging](docs/release/PACKAGING.md). The x86_64 preview passed all six extracted-cartridge manual checks with exit
status 0 per the user. ARM64 extracted-package acceptance passed at `409c505`:
all libraries resolve, all cartridge checks pass, repacked Pong launches/pauses/
restarts, and synth listening and applicable pause/resume pass. Both final native
archives must be regenerated from the same accepted release commit.

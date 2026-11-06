# SugarBee 0.1.0-rc1 — draft release notes

**Not tagged or published.** Proposed source release; final version, contents
and acceptance are pending. See the [readiness checklist](docs/development/release-checklist.md).

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
Pi Release acceptance and
final RC acceptance remain pending; earlier Debug/listening results do not
substitute for those checks. No final RC commit is identified yet.

## Distribution and attribution

Proposed distribution: a source archive plus these notes, with the repository's
LICENSE, third-party notices, examples and build instructions retained.
SugarBee's original code uses the zlib license. Font attribution/provenance is
in `third_party/font8x8/`; the musical lab records its score source in its README.
WAV Lab samples are generated from original test code. System dependencies are
installed separately and retain their own licenses. Binary packaging is not
part of this proposal.

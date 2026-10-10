# SugarBee

SugarBee is an experimental 2D-first homebrew game-console runtime, initially
for Raspberry Pi 4 (64-bit Linux). The primary developer environment is Debian
12 x86_64. Games should target SugarBee services rather than SDL or Linux APIs.

**Status:** early prototype / API not stable. The graphics calls now use a
language-neutral C interface with an SDL2 backend; input, timing, and other
services have not yet been extracted. Lua 5.4 is the first game language.

## Build

See [dependencies](docs/development/dependencies.md) and
[building](docs/development/building.md). The baseline uses CMake, SDL2, Lua
5.4, minizip, zlib, and the system `zip` command for cartridge packaging.

## Documentation

- [Build and testing](docs/development/building.md)
- [Dependencies](docs/development/dependencies.md)
- [Current `.vcart` format](docs/specifications/cartridge-format.md)
- [2D graphics API](docs/api/graphics.md)
- [Current architecture](docs/architecture/overview.md)
- [Imported historical design](docs/architecture/project_sugarbee_technical_design_specification.md)

The historical design files are retained as references; not all their
proposals describe the present implementation.

## Repository policy

Engine changes, documentation, and relevant tests should be updated in the same
pull request. Release and developer builds share a source tree; appliance-image
profiles are future work.

**Licensing:** The `LICENSE` placeholder has not yet been populated with an
approved open-source license. Please settle licensing before public releases
or accepting third-party code contributions.

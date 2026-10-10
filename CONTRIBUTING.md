# Contributing to SugarBee

SugarBee is in the prototyping stage. Please keep patches small and focused.

1. Configure and build using [the build guide](docs/development/building.md).
2. Run `ctest --test-dir build --output-on-failure`.
3. Update API, format, dependency, or behavior documentation **in the same
   change** as the implementation.
4. Update or add an executable example/test when observable behavior changes.
5. Explain any new runtime dependency and its impact on the future minimal
   Raspberry Pi release image.

There is no stable public ABI yet. Changes to the cartridge format require
careful review and corresponding format tests.

The project's open-source license is not selected yet; do not submit
third-party code without checking its provenance and license compatibility.

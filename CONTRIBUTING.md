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

## Licensing and provenance

SugarBee's original code is available under the [zlib license](LICENSE).
Contribute original project code under those terms, and submit only material
that you have the right to contribute. Preserve applicable copyright and
license notices; do not relabel third-party material as SugarBee-owned code.

Before adding third-party code, fonts, images, or other assets, verify their
source and applicable terms. Include the source URL, a pinned version or
revision where available, required notices, and any local adaptations. Record
that information alongside the material or in `third_party/`; the
[embedded font provenance](third_party/font8x8/README.md) is an example.
Dependencies and third-party assets retain their own terms, which must be
compatible with their intended distribution and use.

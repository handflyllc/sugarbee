# SugarBee virtual cartridge (`.vcart`), format v1 — current prototype

**Status: provisional.** Authoritative implementation: `include/vcart.h`,
`src/vcart.c`, `tools/sbee-pack.c`, and `tests/test_vcart.c`. The old design
document describes an earlier layout and `VCAR` magic; those are obsolete.
The agreed magic is **`SBEE`**.

A file is a **64-byte binary header** followed immediately by a **ZIP archive**.
The 64-byte prefix is not part of the normal ZIP specification. The current
loader uses minizip's `unzOpen` on the complete file; offset handling and
robust archive validation still need to be made explicit.

| Offset | Bytes | Name | Encoding |
| --- | ---: | --- | --- |
| `0x00` | 4 | Magic | ASCII `SBEE` |
| `0x04` | 2 | Header version | unsigned little-endian, currently `1` |
| `0x06` | 1 | Min runtime major | unsigned byte |
| `0x07` | 1 | Min runtime minor | unsigned byte |
| `0x08` | 1 | Min runtime patch | unsigned byte |
| `0x09` | 3 | Hardware flags | reserved for future capabilities |
| `0x0c` | 4 | ZIP archive size | unsigned little-endian byte count |
| `0x10` | 16 | Game UUID | RFC 4122 UUIDv4 bytes |
| `0x20` | 32 | Game title | NUL-terminated ASCII-compatible bytes |

Current header checks enforce the magic, header version, minimum runtime
version (lexicographic major/minor/patch comparison), title termination, and
minimum file bounds. Higher major runtime versions are **assumed** compatible
for this check, but this is not a future ABI guarantee. The eventual game API
version and runtime implementation version should be separated.

`payload_size` is the **compressed ZIP archive length**, not the sum of
uncompressed contents. Per-file uncompressed sizes are described by ZIP
entries. Uncompressed limits, file-count/path limits, CRC/integrity checks,
and bounded decompression are required before cartridges can be treated as
untrusted inputs.

## Persistent game identity

A UUID identifies a **game**, not an individual package build. Preserve the
same identifier through updates so the runtime can associate save data with
the game. `sbee-pack` generates a secure random UUIDv4 unless `--uuid` is
provided. Developers must presently retain the value and pass `--uuid` on
subsequent builds. The future game manifest should persist it automatically.

## Planned extensibility

A manifest in the ZIP will eventually describe execution runtime (`lua`
initially), entry point, API version, logical resolution, and optional
capabilities. Do not interpret this planned structure as an implemented feature.

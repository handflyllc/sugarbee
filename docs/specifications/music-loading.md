# Music loading checkpoint

Accepted 2026-11-08; memory-only loader implemented, Pi validation pending.
Canonical coordination: [shared PASSDOWN](../../../sugarbeats/docs/PASSDOWN.md).
Detailed musical plan: [asset plan](../../../sugarbeats/docs/SUGARBEE_MUSIC_ASSET_PLAN.md).

## Scope and dependency

Load one complete song from an explicit-length in-memory JSON buffer into the
existing prepared representation. No cartridge I/O, Lua API, registry, looping,
voice reservations, or preview changes. Use a pinned upstream yyjson release
vendored with its license and provenance under third_party; no build-time fetch.
User-supplied yyjson 0.13.0 is vendored; header version and MIT license checked.
See [provenance](../../third_party/yyjson/README.sugarbee.md) for local checksums
and the upstream-verification limitation. Parser types stay private to loader.

## Accepted validation

- Strict JSON; reject comments, trailing commas, trailing non-whitespace data,
  duplicate decoded keys (including escaped aliases), unknown/missing fields,
  incorrect types, invalid encoding, overflow, and nonfinite values. No coercion,
  sorting, clamping, or default instrument settings.
- Require format sugarbee.music, version 1, timeline_rate 48000.
- Integer fields use plain nonnegative integer notation, without a minus sign,
  fractional part, or exponent. Parse exactly, never through floating point.
- One to nine tracks, each with one fixed complete instrument. Empty events and
  unused tracks are allowed; declared duration remains positive.
- Waveforms: triangle, saw, pulse, noise. Attack/decay/release 0–10 seconds;
  sustain/volume 0–1; pan -1–1; filter cutoff 20–20000 Hz even when bypassed;
  bypass boolean. Pulse width 0.05–0.95, required for pulse, forbidden otherwise.
- Pitched note_on requires hz in 110–880. Noise note_on and all note_off events
  forbid hz. Track indices must exist.
- Canonical event ordering: frame, off before on, then track. Reject duplicates
  of the same event kind on one track at one frame. Each on pairs with a later
  off; no overlapping held notes per track. Retrigger during release is allowed.
- Positive end_frame; no events after it. One-shot release tails may extend
  beyond end_frame. Loop eligibility is explicitly deferred, not implied by load.

## Provisional limits

| Resource | Ceiling |
| --- | --- |
| Input bytes | 1 MiB |
| Events | 4096 (existing prototype ceiling) |
| Declared duration | 28800000 frames / 10 minutes at 48 kHz |
| Container depth | 5, counting root object as 1 |
| Temporary parser allocations | 8 MiB |
| Prepared song allocation | 256 KiB |

These are checkpoint budgets, not measured final soundtrack capacity. Verify
maximum fixtures and actual allocations on desktop and Pi. Input, parser memory,
and prepared storage are distinct budgets. Enforce resource bounds before
unbounded work/allocation, including malformed deeply nested input. Allocation
failure must cleanly release all temporary state. 4096 events permits at most
2048 note pairs across all tracks; revisit with representative SugarBeats songs,
loading-memory measurements and Pi performance. Registry-wide limits are deferred.

## Ownership and diagnostics

Successful loading owns self-contained immutable instruments/events; original
input and parser data can be discarded. Playback borrows the prepared song;
owner keeps it alive until idle/reset. Failure publishes no song and changes no
playback state. Return bounded diagnostics identifying category and field/index
(or syntax byte offset), without logging from the callback. Exact native API and
error names remain implementation details. No parser/allocation in rendering.

## Acceptance and handback

- Valid one-note, mixed chord, same-frame retrigger, noise, silent and boundary
  fixtures; exact sample parity against equivalent C fixtures, split rendering,
  natural tails, replay and independence from original JSON-buffer lifetime.
- Reject malformed syntax/encoding, escaped duplicate keys, unknown/missing
  fields, forbidden waveform/event fields, invalid types/ranges/integer forms,
  overflow, ordering/pairing failures, and each resource-limit violation.
- Allocation-failure cleanup and repeated load/free; relevant sanitizers,
  desktop suite and Pi validation, with pending checks recorded honestly.
- Record upstream version/provenance, supported contract, measured memory and
  source commit before handing back to SugarBeats. No additional engine layers.

## Implementation and validation status

Native target `sugarbee_music_load`, interface `src/sb_music_load.h`. The loader
uses strict yyjson raw numeric parsing, exact unsigned-integer conversion and
bounded allocation. A lexical depth guard runs before parsing; it only counts
containers outside strings. yyjson remains the syntax/encoding parser.
Both old and new allocations count against the parser ceiling during growth.
The existing prepared core also validates the final song privately before return.

See [fixtures, usage and validation](../../tests/music-load/README.md).
Desktop full suite passes **92/92**; targeted and ASan/UBSan checks pass
(LeakSanitizer disabled under tracing).
Measured chord parser/prepared bytes: 6202 / 952; maximum-event fixture:
1271499 / 99064. These do not establish final asset capacity or Pi performance.
Pi validation and a source commit for the next SugarBeats pin remain pending.
Dependency acquisition is no longer blocked. No merge or follower implementation
is authorized by this document.

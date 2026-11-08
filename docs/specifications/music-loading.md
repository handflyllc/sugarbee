# Music loading checkpoint

Accepted loader baseline 2026-11-08; desktop and Pi validation accepted.
Loop-eligibility follow-up implemented; Pi acceptance reported 2026-11-08.
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
  beyond end_frame. Successful loading alone does not imply loop eligibility;
  query the cached analysis described below.

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
Pi acceptance reported by the user on 2026-11-08 for prepared candidate
`10c3d805188adc17d95a7f22192986d38951d34c`: loader exit 0; full suite 92/92,
exit 0. Pi allocation figures were not supplied. Next priority is SugarBeats:
exercise this loader through a bounded offline consumer probe before more engine
layers. Review any subsequent documentation-only commit when updating its clean
source pin; the runtime validation baseline is the candidate above.
Dependency acquisition is no longer blocked. No merge or follower implementation
is authorized by this document.

## Loop eligibility — 2026-11-08 checkpoint

Implemented internal query: `sb_music_asset_loop_eligible(asset)` returns a cached
boolean; NULL returns false. False is not a load failure and does not prevent
one-shot playback. No new JSON field, playback option, or sequencer mode is added.
This does not promise a click-free or musically seamless loop.

For each used track, locate its final note-off and require:
`release_frames <= end_frame - final_note_off_frame`. Equality is eligible.
Obtain release_frames through the existing envelope initialization, reusing its
half-up rounding and minimum one frame for positive durations. Never duplicate
seconds-to-frames arithmetic. Zero release at the end is eligible; any positive
release at the end is ineligible. Unused tracks and silent songs are eligible.

Earlier releases are superseded by later retriggers on that track. Attack, decay,
sustain, volume and output silence do not shorten the final release lifecycle.
Current bank filtering precedes the envelope and runs only for active voices;
it adds no independent post-envelope audible tail. Revisit the analysis if that
synth lifecycle changes. All event validation completes before analysis.

The analysis is linear in event/track count, uses fixed-size scratch storage,
and never renders the song. The query requires no allocation or parsing. Asset
ownership is unchanged. Prepared allocation grows by 8 bytes on the desktop:
chord 960 bytes, maximum-event fixture 99072 bytes; parser peaks unchanged.

Validation: `music_loop_eligibility` compares eligibility with actual sequencer
state at the boundary for whole and single-frame buffers; covers equality and
one-frame offsets, zero/tiny/half-frame releases, early note-off during attack,
retrigger, mute, filter, noise, unused tracks, silent and maximum-duration songs,
and a multi-track final release crossing the boundary. Ineligible songs still
play once and finish. Desktop full suite passed **93/93** (104.21 seconds), with no new build
warnings; loader and eligibility ASan/UBSan checks passed with leak detection
disabled under the existing tracing limitation. Pi acceptance reported by the user
2026-11-08 for prepared candidate `93c598c80489577c3a7595154785f1f547dda8e3`:
loop eligibility checks passed; full suite 93/93 passed. No new local test run
or Pi allocation measurements are claimed. Next priority is SugarBeats to verify
the cached query through its offline consumer; actual loop playback stays deferred.

## Native loop playback — 2026-11-08 checkpoint

Authorized deterministic reset at every whole-song boundary. Explicit
`sb_music_sequence_start_loop(sequence, song)` is additive: existing `start`
remains one-shot. Both reject invalid/busy requests without mutation. Loop start
also rejects ineligible songs, even when supplied directly as C fixtures.
Eligibility analysis now lives in the core; the loader caches that same result.

Start validates/prepares a silent configured bank and captures a value snapshot.
At end_frame, ending events are dispatched, the snapshot is restored, and frame
and event cursor return to zero. Next iteration's frame-zero events are dispatched
before its first sample, including when a render buffer ends at the boundary.
There is no inserted sample, fade, gap, crossfade or accumulating total-frame
counter. Callback boundary work is a fixed bank copy and cursor reset, not
configuration, allocation, parsing or a full event scan.

Snapshot restores oscillator/filter state, instrument controls and deterministic
noise initialization. Authors shape seams with releases, attacks and silence
inside the declared song duration. This policy promises repeatable initialization,
not click-free music. Continuous-state loops and automatic fade/gap controls are
deferred. The native test checks equality to concatenated fresh one-shot renders.

While looping, state remains PLAYING and the immutable song/asset must stay alive
until reset. Reset remains immediate cleanup with callback exclusion; no audible
stop, gain, pause, reservation or cartridge/Lua integration is introduced.
Internal sequence layout changes: desktop bank snapshot 3640 bytes, total
sequence 7320 bytes. Consumers must rebuild against their reviewed source pin;
this is not a stable binary ABI. Prepared asset allocation sizes are unchanged.

See [loop validation](../../tests/music-loop.md) for commands, fixtures and manual
acceptance. User confirmed desktop listening, 95 passing tests and exit 0 on
2026-11-08. User confirmed synchronized desktop/Pi candidate
`cc281aa185e7dab7b3c0cabcd8d410dc5395a7a8`, Pi 95 passing tests and listening
acceptance on 2026-11-08. Next priority: SugarBeats native loop consumer checks.
Pi size figures were not supplied; desktop figures above remain desktop-only.

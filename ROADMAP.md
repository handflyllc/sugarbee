# Project SugarBee — Roadmap

Reviewed 2026-11-06 against merged baseline `b304a00`. This is a revisable
plan, not a release schedule or a promise of additional features.

## Accepted checkpoint — prepared native music sequencing

2026-11-07: engine edits authorized for a small isolated sequencer, native
timing tests, and early interval/chord listening. See
[checkpoint and acceptance steps](tests/music-sequence.md). Production music
loading, looping, reservations and Lua integration remain later checkpoints.
Desktop listening and Pi acceptance passed per the user on 2026-11-07.
Pi commit `49d6a8e`: 91 tests passed, listening passed, exit status 0.
SugarBeats follower checkpoints 0B, 1A and 1B are accepted per the shared
passdown: 6/6 automated checks and user-reported desktop listening acceptance.
The bounded in-memory JSON loader is implemented. See [loading contract](docs/specifications/music-loading.md).
The memory-only loader and fixtures are implemented with vendored yyjson 0.13.0.
Desktop full suite passes 92/92; targeted and sanitizer checks pass. Pi acceptance
reported 2026-11-08 for `10c3d80`: loader exit 0, full suite 92/92, exit 0.
SugarBeats offline loader follower passed 7/7 checks at engine pin `8325d9c`,
including input-buffer independence, repeated playback and balanced allocations.

Accepted checkpoint: cached loop-eligibility analysis on loaded assets.
Desktop 93/93 and targeted ASan/UBSan checks pass. Pi acceptance reported by user
2026-11-08 for `93c598c`: eligibility checks passed; full suite 93/93 passed.
Next priority is SugarBeats: bounded offline consumer validation of eligibility.
Actual loop playback stays deferred. Feature remains unmerged.

## Validated RC1 baseline

Implemented and validated on desktop and Raspberry Pi:

- Cartridge loading, bounded asset reads, PNG sprites and 2D graphics.
- Keyboard/controller input, fixed-step timing, launcher, pause and exit.
- Restricted Lua libraries and instruction budgets for ordinary runaway loops.
- Explicit UUID-based saves with bounded reads and synchronized replacement.
- Nine synth voices: triangle, saw, pulse and noise; ADSR, pan/volume, transitions
  and simple low-pass filtering. Pitched notes support 110–880 Hz.
- Eight WAV playbacks with shared loaded assets, one-shots, whole-sound loops,
  pan/volume/stop, and cartridge-owned cleanup.

All 89 post-merge tests passed at `b304a00` per the user. Pi looping validation
at `026aa2c` passed all 89 tests, manual checks and Exit to system status 0.
This evidence does not establish every hardware/input combination or a fresh
Release-build acceptance run. The user confirmed both controllers work during coastline Release acceptance;
disconnect/reconnect coverage has not been separately reported.

## Release documentation cleanup — prepared

Current guidance and API references are reconciled with the implemented
baseline. Historical checkpoints are labeled separately from current behavior.
Local documentation links and whitespace checks pass. This documentation-only
checkpoint awaits commit; no runtime or API behavior changed.

## Historical release preparation record

Superseded by RC1 `cca7c62` / `v0.1.0-rc1`; publication was user-reported on
2026-11-06. The following records preparation, not current remaining work.

- Coastline fresh Release build: all 89 tests pass, no compiler warnings.
  Coastline manual checks passed per the user, including both controllers and
  Exit to system 0. Pi Release acceptance is complete at `f99583a`, with all 89 tests
  passing per the user.
- Both controllers work on coastline; simultaneous two-controller operation
  passed Pi acceptance per the user.
- Source plus x86_64/ARM64 binary contents and provisional API policy accepted.
  x86_64 preview automated and manual extraction checks pass, exit 0;
  ARM64 extracted-package acceptance passed at `409c505` per the user.
  Final same-commit artifacts, merge/tag and publication remain.
- Prepare release notes and tag the exact accepted commit; `v0.1.0-rc1` is the
  accepted target name, not an existing release.

Track evidence and exact remaining gates in the
[release checklist](docs/development/release-checklist.md). Release notes
are in [RELEASE_NOTES.md](RELEASE_NOTES.md). Preparation was committed as `f99583a` so the Pi could fetch the helper.
Release finalization is authorized. Commit the acceptance record and use the
same tagged commit for all final artifacts; publication follows artifact verification.

## Limits and deferred work

The runtime is not a hostile-cartridge sandbox. Native blocking, compilation,
memory exhaustion and repeatedly caught errors are outside general recovery
guarantees. Use trusted cartridges; see [lifecycle limits](docs/api/timing.md).
Save validation does not establish hardware power-loss durability.

Streaming, resonance, broader synth pitch, music-authoring formats, MIDI,
master/group controls and additional game labs are not RC prerequisites.
Actual OS shutdown, bootable appliance images, update/recovery deployment,
analog input and configurable logical resolutions remain separate work.
Persistent logging is deferred unless a concrete need emerges.

Further features require a small agreed scope, automated checks and appropriate
desktop/Pi acceptance. Keep working functionality and cartridge compatibility
intact; do not turn exploratory ideas into requirements.

## Historical milestone record

The entries below preserve the state and test counts at each checkpoint.
“Pending”, “next”, and “future” within this record describe that time, not the
current baseline above. Current behavior belongs in the API documentation;
new milestones should update the summary rather than extend a second roadmap.

### 1. Lua execution-limit validation — completed

- Top-level main.lua, init, update, draw, and shutdown budgets were validated.
- Desktop automated and real-host checks passed. The user confirmed all Pi
  execution-limit cases on commit `873a8b6`, including shutdown and recovery,
  and confirmed Pong and Pac-Man play after recovery.
- The earlier Pi shutdown hang was reproduced on an older main checkout;
  shutdown_runaway passed after switching to the execution-limit branch.

These limits address accidental runaway Lua loops; they are not a complete
sandbox. Retest the relevant cases when budgets or execution paths change.
See [lifecycle documentation](docs/api/timing.md) and
[execution-limit checks](tests/execution-limit/README.md).

### 2. Establish a bounded console reliability baseline — completed

- Confirmed Exit to system is implemented behind the explicit `--console`
  flag. It performs normal engine cleanup and returns to the launching terminal;
  it does not shut down the OS or indicate that power may safely be removed.
- Desktop checks passed. The user confirmed the Pi checks on `fadcc48`:
  all 19 automated tests passed, confirmation and launcher interactions passed,
  Exit to system returned status 0, and the cartridge shutdown callback did not
  run a second time when leaving the idle host.
- Cartridges now use an explicit supported-library set. Coroutine, debug,
  direct OS/file/module access, and cartridge `__gc` registration are excluded.
  Ordinary metatables and `__close` remain supported, with six isolated closing
  regression cases protected by external CTest timeouts.
- On `4e60bf2`, the user confirmed all seven focused library/closing tests and
  all 26 automated tests passed on the Pi. Real-host checks passed for Pong
  and Pac-Man play, pause/resume, game exit and relaunch, execution-limit Guru
  recovery followed by healthy play, and single-invocation shutdown cleanup.
  Exit to system returned status 0. Desktop automated and gameplay checks
  also passed during these checkpoints.

The bounded baseline is validated. Native calls, compilation, memory exhaustion,
and repeatedly caught errors remain outside a general recovery guarantee;
there is no runtime wall-clock timeout or hostile-cartridge sandbox. See the
[lifecycle limits and library policy](docs/api/timing.md).
Actual OS shutdown is deferred to deployment work. Persistent logging remains
deferred unless validation exposes a concrete need for it.

Keep this phase finite. Safely executing deliberately hostile cartridges would
require a separate scope decision, not an assumed extension of loop protection.

### 3. Implement minimal save support — bounded milestone complete

- Follow the [agreed minimal save design](docs/architecture/save-design.md):
  one opaque string up to 64 KiB per UUID, explicit load/save calls, and a
  host-controlled writable location separate from cartridges.
- Use synchronized temporary-file replacement. Distinguish success, failure
  before replacement (previous save untouched), and uncertain durability after
  replacement; do not promise preservation after replacement already occurred.
- Implement bounded storage and failure tests, then Lua bindings and a small lab.
- Validate normal saves, failed and interrupted writes, recovery, and lifecycle
  behavior on desktop and Pi; do not infer power-loss guarantees from these checks.

Done when: the agreed API and failure contract are implemented, documented, and
validated on desktop and Pi. Internal bounded reading and replacement writing
for existing directories, bounded nested preparation beneath a durable base,
and a C prepare-and-write coordinator are implemented with failure tests.
Host save-directory selection is implemented without accessing storage, and a
read-only adapter resolves selected paths for root-anchored preparation.
Internal host load/save calls now connect active-session identity and the
selected directory to storage. Strict string-only Lua save/load bindings and
their automated tests/API documentation are implemented; no automatic saving.
The counter-based interactive save lab and its behavior tests are implemented.
The user confirmed all six desktop interactive checks passed, including explicit
save/reload, zero, pause/resume, and persistence after relaunch. Three controlled
process-interruption/recovery tests also pass on desktop. On Pi commit `525e215`,
the user confirmed all 37 tests and all eight interactive Save Lab checks passed,
including repack persistence and Exit to system returning status 0.

This completes the agreed bounded save milestone. Actual power-loss testing
remains deferred; these checks do not establish hardware durability guarantees.

### 4. Audio implementation — incremental validation history

Audio is the next major creative milestone after the bounded reliability and
minimal-save phases. A custom console OS image is not a prerequisite.

- Use the [settled initial audio design](docs/architecture/audio-design.md) as
  the starting point: WAV playback and nine explicitly controlled,
  SID-inspired synth voices.
- Choose the first implementation slice at that checkpoint, then develop it
  through small sound-focused labs and Pi testing.
- Keep synthesis expressive while keeping music authoring outside the engine.
  SID file import and a DAW are not goals.

The SDL2 silent-output foundation now opens a host-owned device, permits silent
fallback on initialization/open failure, and closes after cartridge shutdown.
All 40 automated tests pass. The user confirmed normal Pong/Save Lab behavior
and one-message unavailable-audio fallback without gameplay disruption. Lua
audio controls remain pending.
A separate one-second diagnostic-tone executable and automated sample tests are
implemented; all 42 tests pass. The user confirmed clean desktop tone output,
correct device selection, no clicks/distortion, and normal exit. Normal host
startup remains silent. On Pi commit `d6c0dd9`, the user confirmed all 42 tests,
clean tone output after selecting HDMI, gameplay/fallback checks, and Exit to
system status 0. The foundation and diagnostic-tone slices are validated on
desktop and Pi. A single native triangle prototype now supports start, pitch
change, and stop; all 44 tests pass. The user confirmed clean desktop tones,
no obvious clicks at pitch changes, and normal exit. On Pi commit `1664043`,
the user confirmed all 44 tests, both listening tests, gameplay checks, and Exit
to system status 0. This bounded native prototype is validated on desktop and
Pi. A standalone linear ADSR component and edge-case tests are now implemented;
triangle integration now captures per-note settings, preserves phase on retrigger,
and separates note-off from short stop. All 45 automated tests pass; the user
confirmed clean desktop ADSR playback, smooth transitions, and normal exit. Pi
validation passed at `710b738`: the user confirmed all 45 tests, clean
ADSR/retrigger and sine output, gameplay checks, and Exit to system status 0.
Native volume/equal-power pan now use 10 ms smoothing with automated checks;
all 47 tests pass. The user confirmed correct desktop pan direction, mostly
smooth volume changes, silent mute, and normal exit. On Pi commit `aa4f408`,
the user confirmed all 47 tests, pan/volume, earlier tones, gameplay, and Exit
to system status 0 passed. HDMI is the preferred future Pi test output; A/V
audio quality is not an acceptance requirement for this milestone. A standalone nine-voice triangle bank now uses fixed 1/9 synth headroom,
independent controls, and automated mixing/isolation checks. Diagnostic
attenuation stays separate. The bank now has SDL integration, locked native controls, callback tests,
and a native listening sequence. Desktop listening is confirmed and all 50
automated tests pass. Pi validation passed at `d71e42d`: all 50 tests, clean
bank/earlier tones, gameplay, and Exit to system status 0.
Full synthesis, Lua controls, and WAV remain future work.
See [bank scope and tests](tests/audio-bank.md). A standalone deterministic
white-noise source now has defined seed/reset behavior and automated checks;
[noise bank integration](tests/audio-noise.md) now includes fixed per-voice
seeds, sequence-preserving active retrigger, automated checks, and a native
listening tool. All 53 automated tests pass; the user confirmed clean desktop
noise playback, correct stereo placement, and normal exit. The user reported
that the Pi checklist for `557b6ba` passed without issues: all 53 tests, noise
and earlier audio, gameplay, and Exit to system status 0.

Done when: each selected audio slice meets its agreed behavior and resource
constraints with automated checks where appropriate and listening tests.
Do not treat the entire audio design as a single implementation step.

### Audio checkpoint continuation

The design is reconciled against implemented behavior. A standalone PolyBLEP
saw at 220–660 Hz now has sample/spectral checks, native bank/SDL integration,
and a listening tool. All 56 tests pass; the user confirmed clean desktop saw
playback, correct stereo placement, and normal exit. Pi validation passed at
`f1982a9`: all 56 tests, saw/earlier tones, gameplay, and Exit to system status 0.
A standalone PolyBLEP pulse now has reset/bounds/DC/spectral checks and 10 ms
live-width ramps with moving-edge correction. Explicit DC conditioning and its
settled/transition checks are implemented. Native bank/SDL integration and a
listening tool are now implemented. All 61 automated tests pass and the user
confirmed all desktop listening checks. Pi validation passed at `a397e26`: all
61 tests, pulse/earlier tones, gameplay, and Exit to system status 0.
The user validated Raspberry Pi OS Lite 64-bit at `51cbc8b`: all 61 tests,
responsive Pong/Pac-Man, clean sine/pulse audio with correct stereo placement,
Save Lab persistence through relaunch/repacking, and Exit to system status 0.
Keyboard and controller felt delayed in the desktop session but responsive in
the TTY; the delay was not reproduced on Lite. The desktop-associated cause
remains undiagnosed, and no measured latency or audio causality is claimed.
Native triangle/saw/pulse waveform transitions are now implemented with phase
preservation, 10 ms linear fades, and defined repeated-request behavior. All 63
tests pass; the user confirmed clean desktop transitions, steady pitch, and
normal exit. The user reported all Pi checks passed for `d35f182`: all 63 tests,
waveform/earlier tones, responsive gameplay, and Exit to system status 0.
Noise transitions are now implemented with fresh-entry seed behavior, retained
pitch, and a frequency-free selected-noise start. The user confirmed clear
tone/noise distinction in the revised one/three/nine-voice desktop listening
comparison; the voice limit is unchanged. Pi validation passed at `29d63ff`: all
65 tests, clean noise transitions/earlier tones, gameplay checks, and Exit to
system status 0. The agreed first pitch expansion, 110–880 Hz, is now implemented
on `feature/audio-pitch-range`; desktop listening passed per the user; Pi audio, Pong/Pac-Man, and Exit to system checks passed per the user;
The user also confirmed a successful Pi build and zero test failures at
`ff61ef6` on 2026-11-02, completing Pi validation.
See [pitch checks](tests/audio-pitch-range.md). Further pitch expansion remains
uncommitted future scope. Filtering, cartridge audio lifecycle and minimal Lua
controls follow as separately agreed checkpoints. Each remains a separately scoped implementation and
validation step; see the [audio sequence](docs/architecture/audio-design.md#agreed-next-sequence--checkpoint-by-checkpoint).

The simple non-resonant per-voice low-pass/bypass checkpoint is implemented with
cutoff control and 10 ms smoothing. Desktop listening passed per the user;
Pi validation passed at `4b64455` on 2026-11-03 per the user: zero test failures,
filter listening, Pong/Pac-Man, pause/resume, and Exit to system status 0.
See [filter checks](tests/audio-filter.md). Native cartridge lifecycle is now
implemented with pause/freeze/resume, failure rejection, and teardown checks;
desktop session listening and Pong/Pac-Man checks passed per the user. Pi
validation passed at `5cf471b` on 2026-11-03 per the user: audio and Pong/Pac-Man
checks, Exit to system status 0, and zero test failures. See
[session checks](tests/audio-session.md). Minimal Lua controls and the basic Synth Lab are implemented; desktop sound checks, exit, and relaunch passed per the user on
2026-11-03. Pi validation passed at `7f28fbd` per the user: sound controls,
pause/resume, exit/relaunch, Pong/Pac-Man, Exit to system 0, and zero test failures. See [audio API](docs/api/audio.md). Resonance and additional
synthesis features are deferred unless a practical game need justifies them.

The musical follow-up is implemented as a separate
[Musical Synth Lab](examples/musical-synth-lab/README.md): the opening four bars
of Eine kleine Nachtmusik, melody only one octave lower, from a verified
public-domain Mutopia score. A small Lua note/duration table and update-driven
timeline use the existing API. Desktop listening and Pi validation are complete per the user (2026-11-03):
Pi commit `683badf` passed playback/lifecycle checks, Pong/Pac-Man, Exit to
system 0, and all 79 tests;
no tracker, accompaniment, or music-file format. The two short Pong sounds are now implemented: a 330 Hz wall blip and a
660 Hz paddle blip, preserving gameplay. Collision and real-host regression
checks cover the existing API integration. All desktop checks passed on
coastline per the user. Pi validation passed at `db58306` on 2026-11-04:
all requested checks, all 83 tests, and Exit to system 0; see [Pong Lab](examples/pong-lab/README.md). The native WAV foundation is now implemented as a separate checkpoint:
bounded PCM decoding, shared assets, eight one-shot playbacks, fixed mixing
headroom, controls and lifecycle. Desktop checks passed on coastline per the
user. Pi validation passed at `b292f82` on 2026-11-04: all 86 tests, WAV
listening, gameplay/lifecycle checks, and Exit to system 0; see [native WAV checks](tests/audio-wav.md). Lua loading and a cartridge WAV Lab are now implemented as a separate
checkpoint. The user confirmed samples, overlap, pause/resume, exit and
relaunch on coastline; Pi validation passed at `604ae3b` on
2026-11-06 per the user: all 89 tests, the WAV Lab/game regression checklist,
and Exit to system status 0. See
[WAV API](docs/api/wav.md) and [WAV Lab](examples/wav-lab/README.md). Whole-sound looping is now implemented as the next checkpoint; all 89 tests and
desktop/Pi acceptance passed, with Pi validation at `026aa2c` on 2026-11-06
per the user. See the WAV Lab README for the checks.
Streaming remains deferred. No composition format or music-authoring system is added.

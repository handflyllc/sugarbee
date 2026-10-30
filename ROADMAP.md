# Project SugarBee — Preliminary Roadmap

Last reviewed: 2026-10-29.

This is a preliminary, revisable plan, not a release schedule or a promise of
features. Review it at each major checkpoint. Choose the next small, testable
step together; new ideas do not automatically become requirements.

## Working foundation

SugarBee currently provides cartridge loading and bounded asset validation,
2D graphics, controller and keyboard input, fixed-step timing, Lua lifecycle
callbacks, a host pause/exit menu, and cartridge discovery and launching.
Invaders Lab, Pong Lab, and the Pac-Man demo help exercise the engine.
The launcher has passed desktop and Raspberry Pi checks.

Designed, implemented, and validated are separate states. In particular, audio
has an agreed initial design but is not implemented. Execution limits are
implemented and validated on desktop and Pi for the documented accidental-loop
cases. The budgets remain provisional, with explicit coverage limitations.

## Preliminary priority order

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

### 4. Begin audio implementation — silent foundation started

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
automated tests pass. Pi validation remains pending;
full synthesis, Lua controls, and WAV remain future work.
See [bank scope and tests](tests/audio-bank.md).

Done when: each selected audio slice meets its agreed behavior and resource
constraints with automated checks where appropriate and listening tests.
Do not treat the entire audio design as a single implementation step.

## Labs alongside engine work

Continue small labs to reinforce concepts and expose real engine needs. Select
each independently, preferably to exercise the current engine milestone.
A lab is a fundamentals exercise, not a commitment to reproduce a full game.

Possible later exercises include Snake, vertical and horizontal scrolling, and
a small four-panel world with movement in any direction. These are options,
not scheduled projects. Needs discovered in a lab become discussion points;
they do not automatically expand engine scope.

## Later deployment evaluation

Debian/Raspberry Pi OS remains the working foundation. When deployment becomes
the active milestone, define requirements for boot behavior, graphics, input,
audio, storage, updates, recovery, and maintenance before choosing an OS or
image-building approach. No alternative distribution is selected or scheduled.

Actual OS shutdown and its deployment permissions are later work, alongside
repeatable development and console images. They must not become
an open-ended prerequisite for starting audio on the existing foundation.

## Ideas and deferred discussions — not scheduled

- Splash artwork, cartridge icons, and directory configuration UI/files.
- Persistent diagnostic logging, unless validation exposes a concrete need
  within the bounded reliability baseline.
- Additional drawing primitives and framerate diagnostics.
- The Invaders firing-rate Easter egg; its exact behavior remains undecided.
- External compositions targeting SugarBee, MIDI integration, and evaluating
  twelve or more synth voices. Consult the audio design before revisiting these.

## At each major checkpoint

1. Record what is implemented, tested, merged, and still outstanding.
2. Review the next priority and give it a small scope and acceptance criteria.
3. Update this roadmap when decisions change; keep detailed specifications and
   test procedures in their existing documents rather than duplicating them here.
4. Keep exploratory ideas separate from agreed work, and use the scope alarm
   before a small milestone grows into a larger project.

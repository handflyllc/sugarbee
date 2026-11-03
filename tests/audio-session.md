# Cartridge audio lifecycle

Native implementation; no Lua audio bindings are introduced here.

| Event | Behavior |
| --- | --- |
| Launch | Clear all voices, ramps, filter history and settings to bank defaults. |
| Pause | Fade over 480 stereo frames (10 ms at 48 kHz), then freeze the complete bank. |
| Resume | Continue the bank with a 480-frame fade-in. |
| Guru | Reject all bank controls immediately; fade to silence and reset. |
| Eligible shutdown | Keep audio access until the callback returns; paused shutdown stays silent. |
| End cartridge | Immediately reset and reject bank controls; retain the SDL device. |
| Host exit | End cartridge, then close the device. |

Repeated pause/resume requests do not restart a fade. Reversals start from the
current gain. The bank advances during fades, including oscillator phase,
envelopes, noise streams, filter history, and parameter ramps. Once fully paused,
none of that state advances. Configuration remains available during eligible
shutdown, but notes issued while paused remain silent.

Failure is sticky until teardown/new launch. Already silent failure clears
immediately. Teardown does not wait for future callback progress, note releases,
or fade completion. It may cut a fade short; queued device output cannot be
retracted. This does not guarantee click-free immediate teardown.

Unavailable audio remains nonfatal; session operations on NULL are safe and bank
controls return false. Standalone native diagnostics remain available on fresh
devices. Legacy host-only tone diagnostics may run between sessions; cartridge
bank controls cannot. Existing diagnostic gain and synth headroom are unchanged.

## Automated checks

```sh
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

`audio_session` drives the real callback with a paused dummy device: exact fade
samples and boundaries, complete bank freeze, continuation, rapid reversal,
callback partitioning, rejected controls, failure/reset/defaults, and paused
shutdown. `audio_host_silence` exercises real host hooks for pause/resume,
runtime Guru, failed loading, shutdown errors, recovery, and device reuse.
Unavailable/open-failure host cases preserve silent gameplay. Test-only native
callbacks supply sounds; this is not end-to-end Lua audio API coverage.
`audio_session_output` runs the listening tool with dummy output.

## Desktop and Pi listening

```sh
./build/sbee-session-test
```

Expect a 440 Hz tone, one second of silence, the resumed tone, failure silence,
then a fresh 880 Hz tone. The final paused-shutdown note must remain silent.
Check for stuck sound, unexpected bursts, and objectionable transition clicks.
Then check Pong/Pac-Man play, pause/resume, game exit/relaunch, Guru recovery,
and Exit to system (status 0). Existing games do not yet generate audio.

Development validation (2026-11-03): Debug build succeeded with no compiler
warnings; all 71 tests passed. The focused session/host checks also passed
after strengthening the shutdown-command assertion.

Desktop acceptance (2026-11-03), user-reported: `sbee-session-test` and
Pong/Pac-Man checks passed for the implementation based on `120c523`.

Pi validation is pending. Record Pi results against the implementation commit;
do not treat dummy output as listening validation.

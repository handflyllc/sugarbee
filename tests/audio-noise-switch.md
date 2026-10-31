# Noise waveform transitions

Native waveform selection now includes noise, with the existing 480-frame
linear crossfade, unchanged 1/9 headroom and separate diagnostic attenuation.
Repeated destination requests do not restart a fade; other busy requests fail
unchanged, without queuing. Idle selection is silent. Lua remains deferred.

## Seed and phase behavior

- A transition into noise starts a fresh sequence using that slot's configured
  seed or fixed default. Seed zero restores the distinct per-slot default.
- A seed edit never resets audible noise, including during a fade. It applies
  to the next fresh start/transition into noise. Active retrigger preserves the
  sequence. Leaving and later returning to noise restarts the configured seed.
- Noise and the pitched source both advance during the fade. The outgoing
  source stops advancing when the fade finishes. Noise has no oscillator phase.
- Returning from noise starts triangle at phase .25, saw at .5, or pulse at 0
  with its stored width applied immediately. It does not resume stale phase.
- Pitched-to-pitched transitions retain their phase-preserving behavior.

## Retained pitch and selected starts

Each voice remembers its last successfully requested pitched frequency, even
while noise plays. A voice that has never received one uses 440 Hz. Bank reset
restores that default. Valid active `pitch` calls accept finite 220–660 Hz and
update the stored value; on steady noise this makes no audible change. During a
fade they also update whichever pitched source is still audible. Idle pitch
calls remain rejected, matching the existing control contract.

`sb_audio_bank_start_selected_noise` / `sb_audio_voice_start_selected_noise`
take only voice identity, not frequency. They require a selected noise waveform
or a fade whose destination is noise. Idle starts reset the seed; active starts
capture the new ADSR and retrigger from current level, preserving stream and
fade deadline. Noise retrigger does not overwrite retained pitch.

The existing frequency-taking `start_selected` requires a pitched destination,
including when fading out of noise. Its frequency updates the pitched source
and retained value. Wrong start-call types fail without mutation. Type-specific
note-ons still reject active cross-type starts and all in-progress fades.

Release, short stop, mute, and pulse-width control behavior remain shared. If a
note ends during a transition, its destination is retained for the next note.
No extra voice is allocated and the envelope/controls advance once per frame.

## Checks

```sh
ctest --test-dir build -R 'audio_noise_switch|audio_waveform$|audio_bank_sdl' --output-on-failure
./build/sbee-noise-switch-test
```

The pure checks compare every sample of all six noise/tonal transition directions
against separately advanced source references. They cover exact fade deadlines,
seed replay/defaults/edits, no-op and busy requests, default/retained/edited pitch,
normal return phases, correct selected-start routing, retrigger, unchanged noise
samples after pitch edits, frozen outgoing sources, release/stop/mute, mixed
nine-voice bounds, render partition independence, and reset. SDL checks exercise
both directions through the real locked controls/callback, verify diagnostic gain,
legacy diagnostic exclusion, and destroy while noise transitions are active.

The native listening sequence plays each pitched waveform at 330 Hz, switches
to noise, stores 550 Hz while noise plays, then returns to the pitched waveform
at 550 Hz. The three passages are left, center, and right. The comparison then
progresses through one, three, and nine centered voices, all using 440 Hz triangle.
Each group plays triangle, noise, then triangle again for one second each, with
release and a quiet gap before the next group. Noise uses distinct default slot
seeds. Per-voice volume, fixed 1/9 headroom, and diagnostic gain stay unchanged;
this is not a loudness-matched comparison across voice counts.

Check whether tone and noise are clearly distinguishable at each count, as well
as clean transitions,
correct stereo placement, the expected higher return pitch, and normal exit.
Noise has different loudness/timbre; stored pitch does not make noise tonal.
The earlier mixed-pitch/mixed-waveform nine-voice passage was difficult for the
user to distinguish from noise. This simpler comparison investigates that
observation; it does not establish that nine voices are excessive or change the
voice limit. The dummy-device test verifies completion only. The user confirmed that the
revised one/three/nine-voice tone/noise comparison is clearly distinguishable.
This supports mix density as an explanation for the earlier observation without
establishing a measured cause. The nine-voice limit remains unchanged. All 65
automated tests passed before the listening-test adjustment; its updated
completion test also passed. Pi validation remains pending.

Standalone bank build (no SDL/Lua host):

```sh
cmake -S . -B /tmp/sugarbee-noise-switch-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-noise-switch-standalone --target test_audio_noise_switch
ctest --test-dir /tmp/sugarbee-noise-switch-standalone -R '^audio_noise_switch$' --output-on-failure
```

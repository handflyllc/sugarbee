# Musical Synth Lab

A separate cartridge plays the opening **four bars** of Mozart's *Eine kleine
Nachtmusik*, K.525, first movement, as a single triangle-voice melody. The basic
[Synth Lab](../synth-lab/README.md) remains available for individual controls.

```sh
cmake --build build -j4
bash tests/musical-synth-lab/prepare-fixture.sh
./build/sugarbee --console ./build/musical-synth-lab/musical-synth-lab.vcart
```

| Control | Action |
| --- | --- |
| A button / J key | Play, or restart from the beginning |
| B button / K key | Stop (wins simultaneous play/stop presses) |
| Start / Enter | Host pause/exit menu |

Launch is silent. The excerpt lasts eight seconds at 120 quarter-note beats per
minute and plays once. Host pause freezes both the Lua timeline and, after its
short fade, the audio voice. Resume continues; holding a note key is unnecessary.
Restart begins the first note immediately. Stop/exit/relaunch leave no old melody
running. An unavailable audio device permits silent playback of the timeline.

## Score and adaptation

Source verified 2026-11-03:

- [Mutopia edition 900](https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=900),
  *Eine kleine Nachtmusik (1st movement)*, W. A. Mozart, KV 525, 1787.
  Mutopia identifies this edition as **Public Domain**, with anonymous maintainer
  and revision ID `Mutopia-2007/01/01-900`.
- [First violin source](https://www.mutopiaproject.org/ftp/MozartWA/KV525/eine-kleine-nachtmusik-mvt1/eine-kleine-nachtmusik-mvt1-lys/violin1.ly),
  first four bars under `primary theme`, in G major and 4/4.

The melody takes the top note of the opening chord, omits the other parts, and
moves every pitch **one octave down** to fit the existing 110–880 Hz range.
Resulting pitches span D4–D5 (approximately 293.66–587.33 Hz). No recording,
modern arrangement, or accompaniment is included. Source rests and rhythmic
values are retained. This is an excerpt ending at bar 4, not a complete cadence.

120 BPM, triangle timbre, envelope, and the 85% note gate are lab performance
choices, not claims about the original's exact performance. Each note releases
a little before its duration ends to make repeated notes distinct. A4=440 Hz
and equal temperament convert the local note numbers to frequencies.

`main.lua` contains a small table of pitches and quarter-note durations; zero
means rest. The note numbers are convenient local notation, not a MIDI engine
API or file format. `update(dt)` advances the timeline. It keeps fractional time
across boundaries and skips elapsed notes after a long update instead of firing
many note-ons at once. It inherits the host's fixed-update timing: under sustained
host overload, music can slow with the simulation. This is not a sample-accurate
or streaming music player. No audio backend or Lua API changes were needed.

## Validation

```sh
ctest --test-dir build --output-on-failure
```

The behavioral regression checks all 18 melody notes, onset times, pitch bounds,
rests, no automatic playback/looping, restart, stop, simultaneous controls,
irregular/large timesteps, silent fallback, and shutdown. Real-host tests pack
and load this cartridge and exercise full playback, bank freeze during pause,
resume, restart, stop, exit and fresh launch, with dummy and unavailable output.
Dummy output establishes execution behavior, not audible quality.

Development validation (2026-11-03): Debug build completed without compiler
warnings; all 79 tests passed.

Desktop listening accepted on 2026-11-03, user-reported for the implementation
based on `7c827e8`: the lab behaved exactly as expected. Pi validation is pending.

Manual checklist:

1. Launch silently; J plays the recognizable opening once, then stops.
2. Restart midway with J; K stops it. Play again after each.
3. Pause midway with Enter; silence while paused, continuation on resume.
4. Exit while playing; relaunch is silent and starts from the beginning on J.
5. Check the original Synth Lab and Pong/Pac-Man. Exit to system returns 0.

Listen for incorrect rhythm, missing notes, unexpected bursts, or stuck audio.
Simple accompaniment remains optional follow-up work; it is not implemented.

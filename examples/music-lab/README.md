# Music Lab

A four-second, hand-authored three-track backing pattern demonstrates the
[experimental music API](../../docs/api/music.md). Triangle chords use voices
1..3; a toggled pulse effect uses voice 9. The authored rests are deliberate;
loop eligibility does not promise a continuous or inaudible seam.

From the repository root after building host and packer:

```sh
./build/sbee-pack -i examples/music-lab -o build/music-lab.vcart -t "Music Lab"
./build/sugarbee build/music-lab.vcart
```

| Controller / keyboard | Action |
| --- | --- |
| A / J | Start at frame zero with selected loop and volume |
| B / K | Stop music; effect continues |
| X / U | Cycle music volume: .8, .3, 0 (mute) |
| Y / I | Toggle independent pulse effect |
| Up/down / W/S | Toggle looping for the next start |
| Start / Enter | Host pause menu: resume or exit |

Start at low speaker volume. Repeated play while active reports `music_busy`.
An absent audio device permits loading but reports `audio_unavailable` on play.
Loop selection does not change current playback. Mute advances time. Exit and
relaunch restore initial settings and release all prior assets/voices.

Use the [listening checklist](../../tests/music-playback.md) on desktop and Pi.
No exported SugarBeats asset, composer/editor work or new synth capability is
part of this example.

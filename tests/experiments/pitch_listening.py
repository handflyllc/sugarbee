#!/usr/bin/env python3
"""Generate offline upper-register candidate/reference pairs; no runtime edits.
Run from any directory: python3 tests/experiments/pitch_listening.py
Outputs reproducible listening fixtures under ignored build/pitch-listening/.
"""
import ctypes as C
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import wave

sys.dont_write_bytecode = True
import numpy as np
from pitch_feasibility import ROOT, RATE, Pulse, Saw, build, render

NOTES = [('A5', 81), ('C6', 84), ('A6', 93), ('C7', 96), ('A7', 105), ('C8', 108)]
FRAMES = int(1.2 * RATE)
FADE = int(.02 * RATE)
TARGET_RMS = .08


def pair(lib, shape, hz, width):
    phase = (np.arange(FRAMES)*hz/RATE + {'triangle': .25, 'saw': .5, 'pulse': 0}[shape]) % 1
    if shape == 'triangle':
        candidate = 1-4*np.abs(phase-.5)
    else:
        state = Pulse() if shape == 'pulse' else Saw()
        args = [C.byref(state), hz] + ([width] if shape == 'pulse' else [])
        assert getattr(lib, f'sb_audio_{shape}_reset')(*args)
        candidate = render(lib, state, FRAMES, conditioned=(shape == 'pulse')).astype(np.float64)
    # Independent additive reference, strictly sub-Nyquist harmonics.
    # Pulse uses the same DC subtraction / half-scale convention as the bank.
    reference = np.zeros(FRAMES)
    for k in range(1, int(np.ceil(RATE/2/hz))):
        angle = 2*np.pi*k*phase
        if shape == 'triangle' and k % 2:
            reference -= 8/np.pi**2*np.cos(angle)/k**2
        elif shape == 'saw':
            reference -= 2/np.pi*np.sin(angle)/k
        elif shape == 'pulse':
            reference += 2/np.pi*np.sin(np.pi*k*width)/k*np.cos(angle-np.pi*k*width)
    return candidate, reference


def prepare(signal):
    steady = signal[FADE:-FADE]
    rms = float(np.sqrt(np.mean(steady**2)))
    assert np.isfinite(rms) and rms > 0
    gain = TARGET_RMS/rms
    output = signal*gain
    # Raised-cosine fades are audition-only, not the engine's ADSR.
    ramp = .5-.5*np.cos(np.linspace(0,np.pi,FADE))
    output[:FADE] *= ramp
    output[-FADE:] *= ramp[::-1]
    assert np.isfinite(output).all() and np.max(abs(output)) < .5
    return output, {'source_rms': rms, 'audition_gain': gain,
                    'steady_rms': float(np.sqrt(np.mean(output[FADE:-FADE]**2))),
                    'peak': float(np.max(abs(output)))}


def main():
    destination = ROOT/'build/pitch-listening'
    destination.mkdir(parents=True, exist_ok=True)
    manifest = {'rate': RATE, 'channels': 1, 'sample_bits': 16,
                'order': ['candidate', 'reference'], 'target_rms': TARGET_RMS,
                'files': [], 'source_sha256': {}}
    for path in [Path(__file__), Path(__file__).with_name('pitch_feasibility.py'),
                 Path(__file__).with_name('pitch_candidate.py'),
                 ROOT/'src/sb_audio_pulse.c', ROOT/'src/sb_audio_saw.c',
                 ROOT/'src/sb_audio_triangle.c', ROOT/'src/sb_audio_bank.c']:
        manifest['source_sha256'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='sugarbee-listening-') as temporary:
        lib = build(Path(temporary), True)
        for name, shape, width in [('01-triangle','triangle',.5), ('02-saw','saw',.5),
                                    ('03-pulse-50','pulse',.5), ('04-pulse-05','pulse',.05),
                                    ('05-pulse-95','pulse',.95)]:
            chunks = [np.zeros(RATE//2)]
            cursor = RATE//2
            events = []
            for note, midi in NOTES:
                hz = 440*2**((midi-69)/12)
                for kind, signal in zip(['candidate','reference'], pair(lib,shape,hz,width)):
                    output, levels = prepare(signal)
                    events.append(dict(note=note, hz=hz, kind=kind, start_seconds=cursor/RATE, **levels))
                    gap = int((.3 if kind == 'candidate' else .6)*RATE)
                    chunks.extend([output,np.zeros(gap)])
                    cursor += FRAMES+gap
            pcm = np.rint(np.concatenate(chunks)*32767).astype('<i2')
            path = destination/(name+'.wav')
            with wave.open(str(path),'wb') as stream:
                stream.setnchannels(1)
                stream.setsampwidth(2)
                stream.setframerate(RATE)
                stream.writeframes(pcm.tobytes())
            with wave.open(str(path),'rb') as stream:
                assert stream.getnframes() == cursor and stream.getframerate() == RATE
                decoded = np.frombuffer(stream.readframes(cursor),dtype='<i2')/32767
                assert max(abs(decoded)) < .5
                for event in events:
                    at = round(event['start_seconds']*RATE)
                    rms = np.sqrt(np.mean(decoded[at+FADE:at+FRAMES-FADE]**2))
                    assert abs(rms-TARGET_RMS) <= .5/32767  # PCM rounding bound
            manifest['files'].append({'file': path.name, 'seconds': cursor/RATE,
                                      'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'events': events})
            print(f'{path}: {cursor/RATE:.1f}s, verified PCM levels and timing')
    (destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__ == '__main__':
    main()

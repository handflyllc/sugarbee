#!/usr/bin/env python3
"""Offline investigation only. Requires cc and NumPy; never edits engine sources.
Builds temporary pulse/saw copies with experimental range/state/correction edits.
Triangle uses the engine's direct mathematical waveform, not the voice envelope.
"""
import ctypes as C
from pathlib import Path
import subprocess
import tempfile
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
RATE = 48000
TOP = 440 * 2 ** ((108 - 69) / 12)
N = 8192

class Pulse(C.Structure):
    _fields_ = [(x, C.c_double) for x in ('phase', 'frequency', 'width', 'width_from', 'width_target')] + [('width_left', C.c_uint)] + [(x, C.c_double) for x in ('previous_step', 'previous_falling_step')]

class Saw(C.Structure):
    _fields_ = [('phase', C.c_double), ('frequency', C.c_double)]

from pitch_candidate import candidate_source

def build(folder, candidate):
    pulse = (ROOT / 'src/sb_audio_pulse.c').read_text()
    saw = (ROOT / 'src/sb_audio_saw.c').read_text()
    if candidate:
        pulse = candidate_source('sb_audio_pulse.c', pulse)
        saw = candidate_source('sb_audio_saw.c', saw)
    name = 'candidate' if candidate else 'baseline'
    p, s = folder / (name + '_pulse.c'), folder / (name + '_saw.c')
    p.write_text(pulse)
    s.write_text(saw)
    libpath = folder / (name + '.so')
    subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', '-I', str(ROOT/'src'), str(p), str(s), '-lm', '-o', str(libpath)], check=True)
    lib = C.CDLL(str(libpath))
    for shape, state in [('pulse', Pulse), ('saw', Saw)]:
        reset = getattr(lib, f'sb_audio_{shape}_reset')
        reset.argtypes = [C.POINTER(state), C.c_double] + ([C.c_double] if shape == 'pulse' else [])
        reset.restype = C.c_bool
        for suffix in ['render'] + (['render_conditioned'] if shape == 'pulse' else []):
            fn = getattr(lib, f'sb_audio_{shape}_{suffix}')
            fn.argtypes = [C.POINTER(state), C.POINTER(C.c_float), C.c_size_t]
            fn.restype = C.c_bool
    for suffix in ['pitch', 'width']:
        fn = getattr(lib, 'sb_audio_pulse_' + suffix)
        fn.argtypes = [C.POINTER(Pulse), C.c_double]
        fn.restype = C.c_bool
    return lib

def render(lib, state, n, conditioned=False):
    out = np.empty(n, dtype=np.float32)
    shape = 'pulse' if isinstance(state, Pulse) else 'saw'
    fn = getattr(lib, 'sb_audio_' + shape + ('_render_conditioned' if conditioned else '_render'))
    assert fn(C.byref(state), out.ctypes.data_as(C.POINTER(C.c_float)), n)
    assert np.isfinite(out).all()
    return out

def static(lib, shape, hz, width=.5):
    phase = (np.arange(N)*hz/RATE + {'triangle': .25, 'saw': .5, 'pulse': 0}[shape]) % 1
    if shape == 'triangle':
        out = 1-4*np.abs(phase-.5)
    else:
        state = Pulse() if shape == 'pulse' else Saw()
        assert getattr(lib, f'sb_audio_{shape}_reset')(C.byref(state), hz, *([width] if shape == 'pulse' else []))
        out = render(lib, state, N)
    ref = np.full(N, 2*width-1 if shape == 'pulse' else 0.)
    for k in range(1, int(np.ceil(RATE/2/hz))):
        angle = 2*np.pi*k*phase
        if shape == 'triangle' and k % 2:
            ref -= 8/np.pi**2*np.cos(angle)/k**2
        elif shape == 'saw':
            ref -= 2/np.pi*np.sin(angle)/k
        elif shape == 'pulse':
            ref += 4/np.pi*np.sin(np.pi*k*width)/k*np.cos(angle-np.pi*k*width)
    # Includes passband droop AND aliasing; this is not an alias-only metric.
    error = np.sqrt(np.mean((out-ref)**2))
    return error, np.max(np.abs(out))

def moving(lib, hz, split=False, jumps=False, conditioned=False):
    state = Pulse()
    assert lib.sb_audio_pulse_reset(C.byref(state), hz, .05)
    pieces = []
    for start, end, width in [(0,480,.95),(480,1024,.95),(1024,1504,.05),(1504,2048,.05)]:
        assert lib.sb_audio_pulse_width(C.byref(state), width)
        if jumps:
            assert lib.sb_audio_pulse_pitch(C.byref(state), hz if start in (0,1024) else (TOP if hz < 110 else 27.5))
        at = start
        while at < end:
            count = min(37 if split else end-start, end-at)
            pieces.append(render(lib, state, count, conditioned))
            at += count
    return np.concatenate(pieces)

def main():
    with tempfile.TemporaryDirectory(prefix='sugarbee-pitch-') as path:
        old, new = build(Path(path), False), build(Path(path), True)
        # Reuse the existing native moving-width regression unchanged.
        executable = Path(path) / 'pulse_width_test'
        subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-I', str(ROOT/'src'), str(ROOT/'tests/test_audio_pulse_width.c'),
                        str(Path(path)/'candidate_pulse.c'), '-lm', '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print('Static reference RMS error (full-scale units); raw peak')
        for hz in [27.5,110,440,880,1760,3520,TOP]:
            for shape, width in [('triangle',.5),('saw',.5),('pulse',.05),('pulse',.5),('pulse',.95)]:
                error, peak = static(new, shape, hz, width)
                print(f'{hz:.6f} {shape:8s} {width:.2f}: {error:.8f} {peak:.8f}')
        for hz in [110,110.125,220,440,660,879.75,880]:
            for conditioned in [False, True]:
                assert np.array_equal(moving(old,hz,conditioned=conditioned), moving(new,hz,conditioned=conditioned))
        print('Old-band sampled moving-width trajectories: bit-identical float output')
        peak = conditioned_peak = 0.
        for hz in [27.5, 60, 90, 110, 880, 1760, TOP]:
            for jumps in [False, True]:
                for conditioned in [False, True]:
                    a = moving(new,hz,jumps=jumps,conditioned=conditioned)
                    b = moving(new,hz,split=True,jumps=jumps,conditioned=conditioned)
                    assert np.array_equal(a,b)
                    p = np.max(np.abs(a))
                    assert p <= (.95+1e-7 if conditioned else 1+1e-7)
                    if conditioned: conditioned_peak = max(conditioned_peak,p)
                    else: peak = max(peak,p)
        print(f'Moving-width/pitch-jump sampled bounds and partition checks passed: raw {peak:.8f}, conditioned {conditioned_peak:.8f}')
        # Periodic reference uses coherent carrier bins and two PWM ramps.
        # Compare all base-rate positive bins to independently sampled 64x PWM.
        print('Moving PWM: reference-error reduction vs naive (dB), carrier coefficient error')
        size, oversample = 2048, 64
        t = np.arange(size*oversample)/oversample
        w = np.where(t<480,.05+.9*t/480,np.where(t<1024,.95,np.where(t<1504,.95-.9*(t-1024)/480,.05)))
        for carrier in [2,3,5,37,75,150,178]:
            hz = carrier*RATE/size
            ref = np.where((t*carrier/size)%1 < w,1.,-1.)
            ref_fft = np.fft.rfft(ref)/(size*oversample)
            raw = ref[::oversample]
            out = moving(new,hz)
            cr = np.fft.rfft(out)/size
            rr = np.fft.rfft(raw)/size
            bins = slice(1,size//2)
            improvement = 10*np.log10(np.sum(abs(rr[bins]-ref_fft[bins])**2)/np.sum(abs(cr[bins]-ref_fft[bins])**2))
            print(f'{hz:.6f}: {improvement:.3f}, {abs(cr[carrier]-ref_fft[carrier]):.8f}')

if __name__ == '__main__':
    main()

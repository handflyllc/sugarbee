#!/usr/bin/env python3
"""Isolated pulse edge/DC and unchanged native filter checks. No runtime edits."""
import ctypes as C
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
import numpy as np
from pitch_feasibility import ROOT, RATE, TOP, Pulse, Saw, build, render


def copy(state):
    return Pulse.from_buffer_copy(state)


def reference(state, count=32768):
    # Independent numerical integration of piecewise-linear PWM through a
    # unit-area triangular kernel. This tests the quadratic correction itself,
    # not ideal brick-wall bandlimiting. Midpoints avoid edge tie conventions.
    t = (np.arange(count)+.5)*2/count-1
    step = state.frequency/RATE
    next_width = state.width
    if state.width_left:
        left = state.width_left-1
        next_width = (state.width_from+(state.width_target-state.width_from)*(480-left)/480) if left else state.width_target
    speed = np.where(t<0,state.previous_step,step)
    velocity = np.where(t<0,state.previous_step-state.previous_falling_step,next_width-state.width)
    # Separate edge coordinates avoid catastrophic cancellation when relative
    # falling motion is near zero. Floor differences count the rectangular pulse.
    rising = state.phase+t*speed
    falling = (state.phase-state.width)+t*(speed-velocity)
    raw = 2*(np.floor(rising)-np.floor(falling))-1
    return np.sum(raw*(1-abs(t)))*2/count


def edge_checks(lib):
    worst = 0.
    cases = 0
    # Include backward, zero and forward falling motion, edge coincidences,
    # narrow high-frequency pulses, and prior/future velocity reversals.
    for hz in [27.5,90.,110.,880.,TOP]:
        for width,target in [(.05,.95),(.95,.05),(.5,.5),(.5,.95),(.5,.05)]:
            for previous_hz in [27.5,90.,TOP]:
                for previous_velocity in [-.001875,0,.001875]:
                    if not .05 <= width-previous_velocity <= .95:
                        continue
                    for anchor in [0,width,1]:
                        for offset in [-.04,-.0003,-1e-12,0,1e-12,.0003,.04]:
                            state = Pulse()
                            assert lib.sb_audio_pulse_reset(C.byref(state),hz,width)
                            assert lib.sb_audio_pulse_width(C.byref(state),target)
                            state.phase = (anchor+offset)%1
                            state.previous_step = previous_hz/RATE
                            state.previous_falling_step = previous_hz/RATE-previous_velocity
                            expected = reference(state)
                            got = float(render(lib,state,1)[0])
                            assert abs(got) <= 1+1e-7
                            error = abs(got-expected)
                            worst = max(worst,error)
                            assert error < .0003, (hz,width,target,previous_hz,anchor,offset,got,expected)
                            cases += 1
    # Force exactly zero future falling speed, rather than relying on decimal
    # 90 Hz cancellation, which leaves a tiny floating-point residual.
    zero = Pulse()
    assert lib.sb_audio_pulse_reset(C.byref(zero),50.,.25)
    assert lib.sb_audio_pulse_width(C.byref(zero),.75)
    delta = zero.width_from+(zero.width_target-zero.width_from)/480-zero.width
    zero.frequency = delta*RATE
    assert zero.frequency/RATE-delta == 0
    for offset in [-1e-12,0,1e-12]:
        zero.phase = zero.width+offset
        test = copy(zero)
        expected = reference(test)
        got = float(render(lib,test,1)[0])
        assert abs(got-expected) < .0003
    print('Three exactly stationary falling-edge cases passed')
    print(f'{cases} signed/zero/reversed edge cases: maximum quadrature error {worst:.8f} (gate 0.0003)')
    rng = np.random.default_rng(20261113)
    state = Pulse()
    assert lib.sb_audio_pulse_reset(C.byref(state),27.5,.05)
    peak = 0.
    for _ in range(500):
        assert lib.sb_audio_pulse_pitch(C.byref(state),float(rng.uniform(27.5,TOP)))
        assert lib.sb_audio_pulse_width(C.byref(state),float(rng.uniform(.05,.95)))
        twin = copy(state)
        n = int(rng.integers(1,700))
        a = render(lib,state,n,True)
        b = np.concatenate([render(lib,twin,min(37,n-i),True) for i in range(0,n,37)])
        assert np.array_equal(a,b)
        peak = max(peak,float(max(abs(a))))
        assert peak <= .95+1e-7
    print(f'500 seeded live pitch/width retargets: finite, partition-identical; conditioned peak {peak:.8f}')


def filter_library(folder):
    wrapper = folder/'filter_wrapper.c'
    wrapper.write_text('''#include "sb_audio_filter.h"
#include <stddef.h>
int apply(const float *in, double *out, size_t n, double cutoff, int bypass) {
    sb_audio_filter_t f = {0};
    if (!sb_audio_filter_cutoff(&f, cutoff, false) ||
        !sb_audio_filter_bypass(&f, bypass != 0, false)) return 0;
    sb_audio_filter_start(&f);
    for (size_t i=0; i<n; ++i) out[i]=sb_audio_filter_sample(&f,in[i]);
    return 1;
}
''')
    path = folder/'filter.so'
    subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(ROOT/'src'),str(wrapper),str(ROOT/'src/sb_audio_filter.c'),'-lm','-o',str(path)],check=True)
    lib = C.CDLL(str(path))
    lib.apply.argtypes = [C.POINTER(C.c_float),C.POINTER(C.c_double),C.c_size_t,C.c_double,C.c_int]
    lib.apply.restype = C.c_int
    return lib


def low_checks(lib,filter_lib):
    n = RATE*4
    worst_mean = worst_gain = 0.
    for hz in [27.5,55.,110.]:
        for shape,width in [('triangle',.5),('saw',.5),('pulse',.05),('pulse',.5),('pulse',.95)]:
            if shape == 'triangle':
                phase = (np.arange(n)*hz/RATE+.25)%1
                x = (1-4*abs(phase-.5)).astype(np.float32)
            else:
                state = Pulse() if shape == 'pulse' else Saw()
                assert getattr(lib,'sb_audio_'+shape+'_reset')(C.byref(state),hz,*([width] if shape=='pulse' else []))
                x = render(lib,state,n,shape=='pulse')
            # Last two seconds contain integer cycles at each selected pitch.
            steady = x[2*RATE:]
            carrier = np.exp(-2j*np.pi*hz*np.arange(len(steady))/RATE)
            before = np.mean(steady*carrier)
            for cutoff in [20.,27.5,2000.,20000.]:
                y = np.empty(n,dtype=np.float64)
                assert filter_lib.apply(x.ctypes.data_as(C.POINTER(C.c_float)),y.ctypes.data_as(C.POINTER(C.c_double)),n,cutoff,0)
                assert np.isfinite(y).all() and max(abs(y)) <= max(abs(x))+1e-12
                mean = abs(float(np.mean(y[2*RATE:])))
                worst_mean = max(worst_mean,mean)
                assert mean < 2e-6
                k = 1-np.cos(2*np.pi*cutoff/RATE)
                a = np.sqrt(k*k+2*k)-k
                expected = a/(1-(1-a)*np.exp(-2j*np.pi*hz/RATE))
                measured = np.mean(y[2*RATE:]*carrier)/before
                error = abs(measured-expected)
                worst_gain = max(worst_gain,error)
                assert error < 1e-7
            assert filter_lib.apply(x.ctypes.data_as(C.POINTER(C.c_float)),y.ctypes.data_as(C.POINTER(C.c_double)),n,2000.,1)
            assert np.array_equal(x,y)
    print(f'Low-note settled DC/filter: max absolute mean {worst_mean:.10f}, complex response error {worst_gain:.3g}; bypass exact')
    # A low-pass passes DC: conditioning must happen at the oscillator.
    x = np.ones(n,dtype=np.float32)
    y = np.empty(n,dtype=np.float64)
    assert filter_lib.apply(x.ctypes.data_as(C.POINTER(C.c_float)),y.ctypes.data_as(C.POINTER(C.c_double)),n,20.,0)
    assert abs(y[-1]-1) < 1e-10
    print('Confirmed low-pass passes constant DC; it is not a DC-removal stage')


def main():
    with tempfile.TemporaryDirectory(prefix='sugarbee-edges-') as path:
        folder = Path(path)
        lib = build(folder,True)
        edge_checks(lib)
        low_checks(lib,filter_library(folder))

if __name__ == '__main__':
    main()

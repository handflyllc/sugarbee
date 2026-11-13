"""Temporary-source transformations for isolated experiments, never production."""
TOP = 440 * 2 ** ((108 - 69) / 12)

RESIDUAL = '''static double rising_residual(double phase, double previous, double next) {
    double value = 0, d;
    if (previous > 0 && phase < previous) {
        d = 1-phase/previous; value -= d*d;
    }
    if (previous < 0 && phase > 1+previous) {
        d = 1+(phase-1)/(-previous); value += d*d;
    }
    if (next > 0 && phase > 1-next) {
        d = 1+(phase-1)/next; value += d*d;
    }
    if (next < 0 && phase < -next) {
        d = 1-phase/(-next); value -= d*d;
    }
    return value;
}
'''

def replace(source, old, new, count=1):
    if source.count(old) != count:
        raise RuntimeError('Source changed; review experimental patch: ' + old)
    return source.replace(old, new)


def candidate_source(name, source):
    if name in ('sb_audio_triangle.c', 'sb_audio_saw.c', 'sb_audio_pulse.c'):
        source = replace(source, 'frequency >= 110.0 && frequency <= 880.0',
                         f'frequency >= 27.5 && frequency <= {TOP!r}')
    if name == 'sb_audio_bank.c':
        source = replace(source, 'frequency < 110 || frequency > 880',
                         f'frequency < 27.5 || frequency > {TOP!r}', 2)
    if name == 'sb_music_sequence.c':
        source = replace(source, 'e->hz < 110 || e->hz > 880',
                         f'e->hz < 27.5 || e->hz > {TOP!r}')
    if name == 'sb_audio_pulse.c':
        source = replace(source, 'pulse->previous_step < .025', 'pulse->previous_step < .1')
        source = replace(source, 'pulse->previous_falling_step > 0', 'pulse->previous_falling_step > -.002')
        source = replace(source, 'pulse->previous_falling_step < .025', 'pulse->previous_falling_step < .1')
        start = source.index('static double rising_residual(')
        end = source.index('static bool render(', start)
        source = source[:start] + RESIDUAL + source[end:]
    return source

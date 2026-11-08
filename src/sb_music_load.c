#include "sb_music_load.h"
#include "yyjson.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sb_music_asset {
    sb_music_song_t song;
    bool loop_eligible;
    sb_music_instrument_t instruments[SB_AUDIO_BANK_VOICES];
    sb_music_event_t events[];
};
/* Align returned allocations for all types used by yyjson on supported targets. */
typedef union { size_t size; long double align; void *pointer; } allocation_t;
typedef struct { size_t live, peak; bool limited; } budget_t;
static void *budget_malloc(void *ctx, size_t n) {
    budget_t *b = ctx;
    if (n > SB_MUSIC_LOAD_MAX_PARSER - sizeof(allocation_t) ||
        b->live > SB_MUSIC_LOAD_MAX_PARSER - sizeof(allocation_t) - n) {
        b->limited = true; return NULL;
    }
    allocation_t *p = malloc(sizeof(*p) + n);
    if (!p) return NULL;
    p->size = sizeof(*p) + n;
    b->live += p->size;
    if (b->peak < b->live) b->peak = b->live;
    return p + 1;
}
static void budget_free(void *ctx, void *ptr) {
    if (!ptr) return;
    allocation_t *p = (allocation_t *)ptr - 1;
    ((budget_t *)ctx)->live -= p->size;
    free(p);
}
static void *budget_realloc(void *ctx, void *ptr, size_t old, size_t n) {
    /* Count both buffers during growth so even transient use respects the cap. */
    void *next = budget_malloc(ctx, n);
    if (!next) return NULL;
    if (ptr) memcpy(next, ptr, old < n ? old : n);
    budget_free(ctx, ptr);
    return next;
}
static bool fail(sb_music_load_result_t *r, sb_music_load_code_t c,
                 const char *path, const char *why) {
    r->code = c;
    snprintf(r->message, sizeof(r->message), "%s: %s", path, why);
    return false;
}
/* Only a depth guard, not a second JSON parser. yyjson validates all grammar.
 * Strings/escapes must be skipped so quoted braces never contribute to depth. */
static bool depth_ok(const char *s, size_t n, sb_music_load_result_t *r) {
    unsigned depth = 0; bool quoted = false, escape = false;
    for (size_t j = 0; j < n; ++j) {
        char c = s[j];
        if (quoted) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') {
            if (++depth > SB_MUSIC_LOAD_MAX_DEPTH) {
                r->byte_offset = j;
                return fail(r, SB_MUSIC_LOAD_LIMIT, "$", "container depth exceeds 5");
            }
        } else if ((c == '}' || c == ']') && depth) --depth;
    }
    return true;
}
static bool text_is(yyjson_val *v, const char *s) {
    return yyjson_is_str(v) && yyjson_get_len(v) == strlen(s) &&
           memcmp(yyjson_get_str(v), s, strlen(s)) == 0;
}
/* Fixed schema lists bound duplicate detection; compare decoded lengths too,
 * so embedded NULs cannot alias valid keys. No first-key-wins lookup. */
static bool fields(yyjson_val *obj, const char *const *names, size_t count,
                   size_t required, yyjson_val **out, const char *path,
                   sb_music_load_result_t *r) {
    if (!yyjson_is_obj(obj)) return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "expected object");
    memset(out, 0, count * sizeof(*out));
    yyjson_obj_iter it = yyjson_obj_iter_with(obj);
    yyjson_val *key;
    while ((key = yyjson_obj_iter_next(&it))) {
        size_t i;
        for (i = 0; i < count && !text_is(key, names[i]); ++i) {}
        if (i == count) return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "unknown field");
        char location[128]; snprintf(location, sizeof(location), "%s.%s", path, names[i]);
        if (out[i]) return fail(r, SB_MUSIC_LOAD_SCHEMA, location, "duplicate field");
        out[i] = yyjson_obj_iter_get_val(key);
    }
    for (size_t i = 0; i < required; ++i) if (!out[i]) {
        char location[128]; snprintf(location, sizeof(location), "%s.%s", path, names[i]);
        return fail(r, SB_MUSIC_LOAD_SCHEMA, location, "missing field");
    }
    return true;
}
static bool integer(yyjson_val *v, uint64_t min, uint64_t max, uint64_t *out,
                    const char *path, sb_music_load_result_t *r) {
    if (!yyjson_is_raw(v)) return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "expected integer");
    const char *s = yyjson_get_raw(v); size_t n = yyjson_get_len(v); uint64_t value = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9')
            return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "expected unsigned integer notation");
        unsigned digit = (unsigned)(s[i] - '0');
        if (value > max / 10 || (value == max / 10 && digit > max % 10))
            return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "integer outside range");
        value = value * 10 + digit;
    }
    if (!n || value < min) return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "integer outside range");
    *out = value; return true;
}
static bool number(yyjson_val *v, double min, double max, double *out,
                   const char *path, sb_music_load_result_t *r) {
    if (!yyjson_is_raw(v)) return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "expected number");
    yyjson_val parsed;
    /* Raw reader storage is NUL terminated. yyjson's built-in conversion is
     * locale independent and uses no heap for numeric conversion. */
    const char *raw = yyjson_get_raw(v);
    const char *end = yyjson_read_number(raw, &parsed, 0, NULL, NULL);
    if (!end || (size_t)(end - raw) != yyjson_get_len(v))
        return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "invalid or overflowing number");
    double value = yyjson_get_num(&parsed);
    if (!isfinite(value) || value < min || value > max)
        return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "number outside range");
    *out = value; return true;
}
static bool instrument(yyjson_val *track, sb_music_instrument_t *i,
                       const char *path, sb_music_load_result_t *r) {
    const char *const track_names[] = {"instrument"}; yyjson_val *t[1];
    if (!fields(track, track_names, 1, 1, t, path, r)) return false;
    const char *const names[] = {"waveform", "attack", "decay", "sustain", "release",
        "volume", "pan", "filter", "pulse_width"};
    yyjson_val *v[9]; char base[128], loc[160];
    snprintf(base, sizeof(base), "%s.instrument", path);
    if (!fields(t[0], names, 9, 8, v, base, r)) return false;
    if (text_is(v[0], "triangle")) i->waveform = SB_AUDIO_VOICE_TRIANGLE;
    else if (text_is(v[0], "saw")) i->waveform = SB_AUDIO_VOICE_SAW;
    else if (text_is(v[0], "pulse")) i->waveform = SB_AUDIO_VOICE_PULSE;
    else if (text_is(v[0], "noise")) i->waveform = SB_AUDIO_VOICE_NOISE;
    else return fail(r, SB_MUSIC_LOAD_SCHEMA, base, "invalid waveform");
    double *dest[] = {&i->envelope.attack, &i->envelope.decay, &i->envelope.sustain,
        &i->envelope.release, &i->volume, &i->pan};
    const double lo[] = {0,0,0,0,0,-1}, hi[] = {10,10,1,10,1,1};
    for (size_t k = 0; k < 6; ++k) {
        snprintf(loc, sizeof(loc), "%s.%s", base, names[k+1]);
        if (!number(v[k+1], lo[k], hi[k], dest[k], loc, r)) return false;
    }
    snprintf(loc, sizeof(loc), "%s.pulse_width", base);
    if (i->waveform == SB_AUDIO_VOICE_PULSE) {
        if (!number(v[8], .05, .95, &i->pulse_width, loc, r)) return false;
    } else if (v[8]) return fail(r, SB_MUSIC_LOAD_SCHEMA, loc, "only valid for pulse");
    const char *const filters[] = {"cutoff", "bypass"}; yyjson_val *f[2];
    snprintf(loc, sizeof(loc), "%s.filter", base);
    if (!fields(v[7], filters, 2, 2, f, loc, r)) return false;
    snprintf(loc, sizeof(loc), "%s.filter.cutoff", base);
    if (!number(f[0], 20, 20000, &i->filter_cutoff, loc, r)) return false;
    snprintf(loc, sizeof(loc), "%s.filter.bypass", base);
    if (!yyjson_is_bool(f[1])) return fail(r, SB_MUSIC_LOAD_SCHEMA, loc, "expected boolean");
    i->filter_bypass = yyjson_get_bool(f[1]); return true;
}
static bool events(yyjson_val *array, sb_music_asset_t *a, sb_music_load_result_t *r) {
    const char *const names[] = {"frame", "track", "type", "hz"};
    bool held[SB_AUDIO_BANK_VOICES] = {false}; uint64_t started[SB_AUDIO_BANK_VOICES] = {0};
    size_t k, count; yyjson_val *obj;
    yyjson_arr_foreach(array, k, count, obj) {
        yyjson_val *v[4]; char path[64], loc[96];
        snprintf(path, sizeof(path), "events[%zu]", k);
        if (!fields(obj, names, 4, 3, v, path, r)) return false;
        sb_music_event_t *e = &a->events[k]; uint64_t track;
        snprintf(loc, sizeof(loc), "%s.frame", path);
        if (!integer(v[0], 0, a->song.end_frame, &e->frame, loc, r)) return false;
        snprintf(loc, sizeof(loc), "%s.track", path);
        if (!integer(v[1], 1, a->song.track_count, &track, loc, r)) return false;
        e->track = (unsigned)track;
        if (text_is(v[2], "note_on")) e->kind = SB_MUSIC_NOTE_ON;
        else if (text_is(v[2], "note_off")) e->kind = SB_MUSIC_NOTE_OFF;
        else return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "invalid event type");
        unsigned slot = e->track - 1;
        snprintf(loc, sizeof(loc), "%s.hz", path);
        if (e->kind == SB_MUSIC_NOTE_ON && a->instruments[slot].waveform != SB_AUDIO_VOICE_NOISE) {
            if (!number(v[3], 110, 880, &e->hz, loc, r)) return false;
        } else if (v[3]) return fail(r, SB_MUSIC_LOAD_SCHEMA, loc, "forbidden for this event");
        if (k) {
            const sb_music_event_t *p = e - 1;
            if (e->frame < p->frame || (e->frame == p->frame &&
                (e->kind < p->kind || (e->kind == p->kind && e->track <= p->track))))
                return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "noncanonical or duplicate event");
        }
        if (e->kind == SB_MUSIC_NOTE_ON) {
            if (held[slot]) return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "overlapping held note");
            held[slot] = true; started[slot] = e->frame;
        } else {
            if (!held[slot] || e->frame <= started[slot])
                return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "unpaired or zero-duration note");
            held[slot] = false;
        }
    }
    for (unsigned j = 0; j < a->song.track_count; ++j) if (held[j]) {
        char path[64]; snprintf(path, sizeof(path), "tracks[%u]", j);
        return fail(r, SB_MUSIC_LOAD_SCHEMA, path, "missing note_off");
    }
    return true;
}
sb_music_asset_t *sb_music_load(const char *json, size_t length, sb_music_load_result_t *result) {
    sb_music_load_result_t local = {0};
    sb_music_load_result_t *r = result ? result : &local;
    *r = (sb_music_load_result_t){0}; r->byte_offset = SIZE_MAX;
    if (!json) { fail(r, SB_MUSIC_LOAD_ARGUMENT, "$", "NULL input"); return NULL; }
    if (length > SB_MUSIC_LOAD_MAX_INPUT) {
        fail(r, SB_MUSIC_LOAD_LIMIT, "$", "input exceeds 1 MiB"); return NULL;
    }
    if (!depth_ok(json, length, r)) return NULL;
    budget_t budget = {0};
    yyjson_alc alc = {budget_malloc, budget_realloc, budget_free, &budget};
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, length, YYJSON_READ_NUMBER_AS_RAW, &alc, &err);
    sb_music_asset_t *a = NULL;
    if (!doc) {
        sb_music_load_code_t code = err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION ?
            (budget.limited ? SB_MUSIC_LOAD_LIMIT : SB_MUSIC_LOAD_MEMORY) : SB_MUSIC_LOAD_SYNTAX;
        r->byte_offset = err.pos;
        fail(r, code, "$", budget.limited ? "parser memory budget exceeded" : err.msg);
        goto done;
    }
    const char *const names[] = {"format", "version", "timeline_rate", "end_frame", "tracks", "events"};
    yyjson_val *v[6]; uint64_t version, rate, end;
    if (!fields(yyjson_doc_get_root(doc), names, 6, 6, v, "$", r)) goto done;
    if (!text_is(v[0], "sugarbee.music")) {
        fail(r, SB_MUSIC_LOAD_SCHEMA, "format", "unsupported format"); goto done;
    }
    if (!integer(v[1], 1, 1, &version, "version", r) ||
        !integer(v[2], 48000, 48000, &rate, "timeline_rate", r) ||
        !integer(v[3], 1, SB_MUSIC_LOAD_MAX_END, &end, "end_frame", r)) goto done;
    if (!yyjson_is_arr(v[4]) || !yyjson_is_arr(v[5])) {
        fail(r, SB_MUSIC_LOAD_SCHEMA, "$", "tracks and events must be arrays"); goto done;
    }
    size_t tracks = yyjson_arr_size(v[4]), count = yyjson_arr_size(v[5]);
    if (!tracks || tracks > SB_AUDIO_BANK_VOICES || count > SB_MUSIC_SEQUENCE_MAX_EVENTS) {
        fail(r, SB_MUSIC_LOAD_LIMIT, "$", "track or event count outside limit"); goto done;
    }
    /* Count is already bounded; retain explicit overflow check at allocation. */
    if (count > (SB_MUSIC_LOAD_MAX_PREPARED - sizeof(*a)) / sizeof(sb_music_event_t)) {
        fail(r, SB_MUSIC_LOAD_LIMIT, "$", "prepared memory budget exceeded"); goto done;
    }
    size_t bytes = sizeof(*a) + count * sizeof(sb_music_event_t);
    a = malloc(bytes);
    if (!a) { fail(r, SB_MUSIC_LOAD_MEMORY, "$", "prepared allocation failed"); goto done; }
    memset(a, 0, bytes);
    a->song = (sb_music_song_t){(unsigned)rate, (unsigned)tracks, end,
                              a->instruments, a->events, count};
    size_t k, n; yyjson_val *track;
    yyjson_arr_foreach(v[4], k, n, track) {
        char path[64]; snprintf(path, sizeof(path), "tracks[%zu]", k);
        if (!instrument(track, &a->instruments[k], path, r)) goto done;
    }
    if (!events(v[5], a, r)) goto done;
    /* Keep the playback core authoritative too; this private temporary cannot
     * reserve voices or alter any caller's live sequencer. */
    sb_music_sequence_t probe = {0};
    if (!sb_music_sequence_start(&probe, &a->song)) {
        fail(r, SB_MUSIC_LOAD_SCHEMA, "$", "prepared core refused song"); goto done;
    }
    a->loop_eligible = sb_music_sequence_loop_eligible(&a->song);
    r->prepared_bytes = bytes;
done:
    yyjson_doc_free(doc);
    r->parser_peak_bytes = budget.peak;
    if (r->code != SB_MUSIC_LOAD_OK) { free(a); a = NULL; }
    return a;
}
const sb_music_song_t *sb_music_asset_song(const sb_music_asset_t *a) { return a ? &a->song : NULL; }
void sb_music_asset_free(sb_music_asset_t *a) { free(a); }

bool sb_music_asset_loop_eligible(const sb_music_asset_t *a) { return a && a->loop_eligible; }

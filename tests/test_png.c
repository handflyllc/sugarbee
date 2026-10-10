#include "sb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "PNG test failed at %d: %s\n", __LINE__, #cond); \
    return 1; } } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2);
    FILE *f = fopen(argv[1], "rb");
    CHECK(f != NULL);
    CHECK(fseek(f, 0, SEEK_END) == 0);
    long n = ftell(f);
    CHECK(n > 0 && n < 1024 * 1024);
    CHECK(fseek(f, 0, SEEK_SET) == 0);
    uint8_t *encoded = malloc((size_t)n);
    CHECK(encoded != NULL);
    CHECK(fread(encoded, 1, (size_t)n, f) == (size_t)n);
    fclose(f);

    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    size_t len = 0;
    CHECK(sb_png_decode_rgba(encoded, (size_t)n, &rgba, &w, &h, &len));
    CHECK(w == 48 && h == 32 && len == 48u * 32u * 4u);
    CHECK(rgba[3] == 0); /* transparent upper-left pixel */
    CHECK(rgba[((size_t)18 * 48 + 20) * 4 + 3] == 255);
    free(rgba);
    rgba = NULL;
    CHECK(!sb_png_decode_rgba(encoded, 8, &rgba, &w, &h, &len));
    CHECK(rgba == NULL && w == 0 && h == 0 && len == 0);
    static const uint8_t fake[] = "not a png";
    CHECK(!sb_png_decode_rgba(fake, sizeof(fake), &rgba, &w, &h, &len));
    free(encoded);
    return 0;
}

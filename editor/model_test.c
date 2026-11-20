#include "vc10.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct vc_project project;
static char input[VC_BYTES], output[VC_BYTES];

static void placement_checks(void)
{
    struct vc_project before;
    struct vc_cue cut = { 0 }, slide = { 0 }, invalid;
    unsigned effect, a;

    cut.effect = VC_CUT;
    cut.screen = slide.screen = VC_FIRST_SCREEN;
    for (effect = VC_SLIDE; effect <= VC_REVEAL_DOWN; ++effect) {
        for (a = 1; a <= 255; a += 254) {
            vc_init(&project);
            assert(vc_place(&project, 0, &cut));
            slide.effect = effect;
            slide.a = a;
            slide.tick = 50;
            assert(vc_place(&project, 1, &slide));
            assert(vc_length(&slide) == (a == 1 ? 2 : 129));
            memcpy(&before, &project, sizeof(before));
            invalid = slide;
            invalid.a = 0;
            assert(!vc_place(&project, 1, &invalid));
            invalid.a = 256;
            assert(!vc_place(&project, 1, &invalid));
            invalid.a = 65535;
            assert(!vc_place(&project, 1, &invalid));
            invalid = slide;
            invalid.tick = 0;
            assert(!vc_place(&project, 1, &invalid));
            invalid.tick = VC_TICK_MAX;
            assert(!vc_place(&project, 1, &invalid));
            invalid.tick = VC_TICK_MAX + 1;
            assert(!vc_place(&project, 1, &invalid));
            invalid = cut;
            invalid.tick = 50;
            assert(!vc_place(&project, 2, &invalid));
            invalid = slide;
            invalid.effect = 0;
            assert(!vc_place(&project, 1, &invalid));
            invalid.effect = 10;
            assert(!vc_place(&project, 1, &invalid));
            invalid.effect = 900;
            assert(!vc_place(&project, 1, &invalid));
            assert(!memcmp(&project, &before, sizeof(project)));
            invalid = cut;
            invalid.tick = 50 + vc_length(&slide);
            assert(vc_place(&project, 2, &invalid));
            invalid.tick = 49;
            assert(vc_place(&project, 3, &invalid));
            slide.tick = VC_TICK_MAX + 1 - vc_length(&slide);
            assert(vc_place(&project, 2, &slide));
        }
    }
}

int main(int argc, char **argv)
{
    FILE *file;
    size_t size;
    const char *error;
    struct vc_frame frame;
    unsigned long tick;
    unsigned i;

    placement_checks();
    if (argc != 3 || !(file = fopen(argv[1], "rb")))
        return 2;
    size = fread(input, 1, sizeof(input), file);
    if (ferror(file) || !feof(file)) {
        fclose(file);
        return 2;
    }
    fclose(file);
    if ((error = vc_read(&project, input, size))) {
        puts(error);
        return 1;
    }
    if (!(file = fopen(argv[2], "wb")))
        return 2;
    size = vc_write(&project, output);
    if (fwrite(output, 1, size, file) != size || fclose(file))
        return 2;
    for (i = 0; i < project.cues; ++i) {
        unsigned long start = project.cue[i].tick;
        unsigned long end = start + vc_length(&project.cue[i]) + 1;
        for (tick = start ? start - 1 : 0; tick <= end; ++tick) {
            frame = vc_at(&project, tick);
            printf("%lu %u %u %u %u %u\n", tick, frame.top, frame.bottom,
                   frame.split, frame.top_row, frame.bottom_row);
        }
    }
    return 0;
}

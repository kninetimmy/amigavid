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
    for (effect = VC_SLIDE; effect <= VC_SQUEEZE_ON_DOWN; ++effect) {
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
            invalid.effect = 14;
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

static void peel_checks(void)
{
    struct vc_cue cue = { 0 };
    struct vc_frame frame;
    unsigned effect;

    for (effect = VC_PEEL_UP; effect <= VC_PEEL_DOWN; ++effect) {
        vc_init(&project);
        cue.tick = 0;
        cue.effect = effect;
        cue.screen = 7;
        cue.a = 255;             /* One row per field, one startup update. */
        assert(vc_place(&project, 0, &cue));
        frame = vc_at(&project, 63);
        assert(frame.reflect == (effect == VC_PEEL_UP ? 128 : 127));
        assert(frame.split == (effect == VC_PEEL_UP ? 256 : 254));
        cue.tick = 1;            /* Two updates per tick; opposite parity. */
        assert(vc_place(&project, 0, &cue));
        frame = vc_at(&project, 64);
        assert(frame.reflect == (effect == VC_PEEL_UP ? 127 : 128));
        assert(frame.split == (effect == VC_PEEL_UP ? 254 : 256));
        frame = vc_at(&project, 128);
        assert(frame.top == 7 && frame.split == 256 && frame.reflect == 256);
        cue.tick = 130;          /* First legal tick after the reserved length. */
        cue.effect = VC_SLIDE;
        cue.screen = 6;
        cue.a = 19;
        assert(vc_place(&project, 1, &cue));
        frame = vc_at(&project, 130);
        assert(frame.top == 7 && frame.bottom == 6 && frame.split == 241);
        assert(frame.reflect == 256 && !frame.top_row && !frame.bottom_row);
    }
}

static void squeeze_checks(void)
{
    struct vc_cue cue = { 0 };
    struct vc_frame frame;
    unsigned effect;

    for (effect = VC_SQUEEZE_ON_UP; effect <= VC_SQUEEZE_ON_DOWN; ++effect) {
        vc_init(&project);
        cue.tick = 1;
        cue.effect = effect;
        cue.screen = 7;
        cue.a = 25;
        assert(vc_place(&project, 0, &cue));
        frame = vc_at(&project, 1);
        /* CopLoop DIVU at outgoing heights 243 (Up) / 244 (Down). */
        assert(frame.top_step == (effect == VC_SQUEEZE_ON_UP ? 134 : 128));
        assert(frame.bottom_step == (effect == VC_SQUEEZE_ON_DOWN ? 134 : 128));
        cue.tick = 23;           /* First legal tick after the 22-tick reservation. */
        cue.effect = VC_SLIDE;
        cue.screen = 6;
        cue.a = 19;
        assert(vc_place(&project, 1, &cue));
        frame = vc_at(&project, 23);
        assert(frame.top == 7 && frame.bottom == 6 && frame.split == 241);
        assert(!frame.top_row && !frame.bottom_row);
        assert(frame.top_step == 128 && frame.bottom_step == 128);
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
    peel_checks();
    squeeze_checks();
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

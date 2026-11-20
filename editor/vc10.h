#ifndef VC10_H
#define VC10_H

#include <stddef.h>

#define VC_WIDTH 320
#define VC_HEIGHT 256
#define VC_FIRST_SCREEN 6
#define VC_SCREENS 94             /* SaveVideo emits at most screen ID 99. */
#define VC_CUES 128
#define VC_TICK_MAX 65535UL      /* CanEffectGoHere takes a UWORD time. */
#define VC_BYTES 65536
#define VC_NAME 32
#define VC_TICK_MS 40
#define VC_CUT 1
#define VC_SLIDE 2
#define VC_SLIDE_ON_DOWN 3
#define VC_SLIDE_OFF_UP 4
#define VC_SLIDE_OFF_DOWN 5
#define VC_SCROLL_UP 6
#define VC_SCROLL_DOWN 7
#define VC_REVEAL_UP 8
#define VC_REVEAL_DOWN 9
#define VC_PEEL_UP 10
#define VC_PEEL_DOWN 11

struct vc_screen {
    char first[VC_NAME], second[VC_NAME];
};

struct vc_cue {
    unsigned long tick;
    unsigned effect, screen, a, b, c, d, e;
    char text[VC_NAME];
};

struct vc_project {
    char author[150];
    unsigned track, screens, cues;
    struct vc_screen screen[VC_SCREENS];
    struct vc_cue cue[VC_CUES];
};

struct vc_frame {
    unsigned top, bottom, split;  /* screen IDs (0 is black); split is a row. */
    unsigned top_row, bottom_row; /* source row at the start of each region. */
    unsigned reflect;             /* top rows reverse after this row; 256 is off. */
};

const char *vc_picture_path(const char *name);
void vc_init(struct vc_project *project);
const char *vc_read(struct vc_project *project, const char *data, size_t size);
size_t vc_write(const struct vc_project *project, char data[VC_BYTES]);
unsigned vc_length(const struct vc_cue *cue);
int vc_place(struct vc_project *project, unsigned index, const struct vc_cue *cue);
struct vc_frame vc_at(const struct vc_project *project, unsigned long tick);

#endif

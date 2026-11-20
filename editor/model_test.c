#include "vc10.h"
#include <stdio.h>

static struct vc_project project;
static char input[VC_BYTES], output[VC_BYTES];

int main(int argc, char **argv)
{
    FILE *file;
    size_t size;
    const char *error;
    struct vc_frame frame;
    unsigned long tick;
    unsigned i;

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
            printf("%lu %u %u %u\n", tick, frame.top, frame.bottom, frame.split);
        }
    }
    return 0;
}

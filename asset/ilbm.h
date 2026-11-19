#ifndef AMIGAVID_ILBM_H
#define AMIGAVID_ILBM_H

#include <stddef.h>

#define ILBM_MAX_SIDE 1024
#define ILBM_MAX_FILE (4UL * 1024 * 1024)

struct ilbm_image {
    unsigned width, height;
    unsigned char *rgb;
};

/* Returns NULL on success, otherwise a diagnostic and leaves out unchanged.
 * Release a successful result with ilbm_free. RGB is top-down, tightly packed. */
const char *ilbm_decode(const unsigned char *data, size_t size, struct ilbm_image *out);
void ilbm_free(struct ilbm_image *image);

#endif

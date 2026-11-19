/* Host CLI for testing the same decoder that is linked into the viewer. */
#include "ilbm.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *file;
    unsigned char *data;
    struct ilbm_image image;
    const char *problem;
    size_t got, size;
    int status;

    if (argc != 3) {
        fprintf(stderr, "Usage: decode_test input output.rgb\n");
        return 2;
    }
    data = malloc(ILBM_MAX_FILE + 1);
    if (!data)
        return 2;
    file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        free(data);
        return 2;
    }
    size = fread(data, 1, ILBM_MAX_FILE + 1, file);
    status = ferror(file);
    fclose(file);
    if (status) {
        fprintf(stderr, "Cannot read %s\n", argv[1]);
        free(data);
        return 2;
    }
    problem = ilbm_decode(data, size, &image);
    free(data);
    if (problem) {
        fprintf(stderr, "%s\n", problem);
        return 1;
    }
    file = fopen(argv[2], "wb");
    if (!file) {
        perror(argv[2]);
        ilbm_free(&image);
        return 2;
    }
    size = (size_t)image.width * image.height * 3;
    got = fwrite(image.rgb, 1, size, file);
    status = fclose(file);
    ilbm_free(&image);
    if (got != size || status) {
        fprintf(stderr, "Cannot write %s\n", argv[2]);
        return 2;
    }
    printf("%u %u\n", image.width, image.height);
    return 0;
}

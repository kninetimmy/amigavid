/* Bounded decoder for ordinary indexed FORM ILBM, independent of Win32. */
#include "ilbm.h"
#include <stdlib.h>
#include <string.h>

static unsigned be16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)be16(p) << 16) | be16(p + 2);
}

static int read_row(unsigned char *dst, unsigned len, int compressed,
                    const unsigned char **cursor, const unsigned char *end)
{
    const unsigned char *p = *cursor;
    unsigned control, count;

    if (!compressed) {
        if ((size_t)(end - p) < len)
            return 0;
        memcpy(dst, p, len);
        p += len;
    } else {
        while (len) {
            if (p == end)
                return 0;
            control = *p++;
            if (control == 128)
                continue;
            count = control < 128 ? control + 1 : 257 - control;
            if (count > len)
                return 0;  /* ByteRun1 packets cannot cross plane rows. */
            if (control < 128) {
                if ((size_t)(end - p) < count)
                    return 0;
                memcpy(dst, p, count);
                p += count;
            } else {
                if (p == end)
                    return 0;
                memset(dst, *p++, count);
            }
            dst += count;
            len -= count;
        }
    }
    *cursor = p;
    return 1;
}

const char *ilbm_decode(const unsigned char *data, size_t size, struct ilbm_image *out)
{
    const unsigned char *bmhd = NULL, *cmap = NULL, *body = NULL, *camg = NULL;
    const unsigned char *chunk, *p, *end;
    size_t pos, colors = 0, body_size = 0;
    unsigned long len;
    unsigned width, height, planes, row_bytes, x, y, plane, index, bit;
    unsigned char rows[8][ILBM_MAX_SIDE / 8], *rgb, *dst;
    int compressed;

    if (size < 12 || size > ILBM_MAX_FILE || memcmp(data, "FORM", 4) != 0 ||
        memcmp(data + 8, "ILBM", 4) != 0)
        return "Expected FORM ILBM within the file size limit.";
    if (be32(data + 4) != size - 8)
        return "Incorrect FORM length.";

    for (pos = 12; pos < size; pos += 8 + (size_t)len + (len & 1)) {
        if (size - pos < 8)
            return "Truncated IFF chunk header.";
        chunk = data + pos;
        len = be32(chunk + 4);
        if (len > size - pos - 8 || (len & 1) > size - pos - 8 - len)
            return "Truncated IFF chunk or missing pad byte.";
        if (memcmp(chunk, "BMHD", 4) == 0) {
            if (bmhd || len != 20)
                return "Invalid or duplicate BMHD.";
            bmhd = chunk + 8;
        } else if (memcmp(chunk, "CMAP", 4) == 0) {
            if (cmap || !len || len > 768 || len % 3)
                return "Invalid or duplicate CMAP.";
            cmap = chunk + 8;
            colors = (size_t)len / 3;
        } else if (memcmp(chunk, "BODY", 4) == 0) {
            if (body)
                return "Duplicate BODY.";
            body = chunk + 8;
            body_size = (size_t)len;
        } else if (memcmp(chunk, "CAMG", 4) == 0) {
            if (camg || len != 4)
                return "Invalid or duplicate CAMG.";
            camg = chunk + 8;
        }
    }
    if (!bmhd || !cmap || !body)
        return "Missing BMHD, CMAP or BODY.";
    width = be16(bmhd);
    height = be16(bmhd + 2);
    if (!width || !height || width > ILBM_MAX_SIDE || height > ILBM_MAX_SIDE)
        return "ILBM dimensions outside the 1-1024 range.";
    planes = bmhd[8];
    compressed = bmhd[10];
    if (!planes || planes > 8 || bmhd[9] || compressed > 1 ||
        (camg && (be32(camg) & 0x0880UL)))
        return "Unsupported ILBM planes, masking, compression or HAM/EHB mode.";

    row_bytes = ((width + 15) / 16) * 2;
    rgb = malloc((size_t)width * height * 3);
    if (!rgb)
        return "Not enough memory for ILBM pixels.";
    p = body;
    end = body + body_size;
    dst = rgb;
    for (y = 0; y < height; ++y) {
        for (plane = 0; plane < planes; ++plane)
            if (!read_row(rows[plane], row_bytes, compressed, &p, end))
                goto bad_body;
        for (x = 0; x < width; ++x) {
            bit = 0x80U >> (x % 8);
            index = 0;
            for (plane = 0; plane < planes; ++plane)
                if (rows[plane][x / 8] & bit)
                    index |= 1U << plane;
            if (index >= colors) {
                free(rgb);
                return "Palette index outside CMAP.";
            }
            memcpy(dst, cmap + 3 * index, 3);
            dst += 3;
        }
    }
    while (compressed && p != end && *p == 128)
        ++p;
    /* Some writers count the final word-alignment byte in BODY's size.
       Accept a single zero at an odd offset, only after all rows are full. */
    if (compressed && end - p == 1 && ((p - body) & 1) && *p == 0)
        ++p;
    if (p != end)
        goto bad_body;

    out->width = width;
    out->height = height;
    out->rgb = rgb;
    return NULL;

bad_body:
    free(rgb);
    return "Invalid or truncated BODY plane row data.";
}

void ilbm_free(struct ilbm_image *image)
{
    free(image->rgb);
    image->rgb = NULL;
}

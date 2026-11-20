#include "vc10.h"
#include <stdio.h>
#include <string.h>

static const char header[] = "VC10\xa9 Almathera Ltd 1994";

static int device_name(const char *name, size_t n)
{
    char stem[8];
    const char *dot = memchr(name, '.', n);
    size_t i;

    if (dot)
        n = dot - name;
    while (n && name[n - 1] == ' ')
        --n;
    if (n >= sizeof(stem))
        return 0;
    for (i = 0; i < n; ++i)
        stem[i] = name[i] >= 'a' && name[i] <= 'z' ? name[i] - 'a' + 'A' : name[i];
    stem[i] = 0;
    return !strcmp(stem, "CON") || !strcmp(stem, "PRN") ||
           !strcmp(stem, "AUX") || !strcmp(stem, "NUL") ||
           !strcmp(stem, "CLOCK$") || !strcmp(stem, "CONIN$") || !strcmp(stem, "CONOUT$") ||
           (i == 4 && (!memcmp(stem, "COM", 3) || !memcmp(stem, "LPT", 3)) &&
            stem[3] >= '0' && stem[3] <= '9');
}

/* Only the picture assign and relative names; preserve the original spelling
   in the model. No Windows drive, ADS, absolute path or parent components. */
const char *vc_picture_path(const char *name)
{
    const char *start, *p, *relative = name;
    static const char assign[] = "vcpics:";
    unsigned i, c;

    if (strlen(name) >= VC_NAME)
        return NULL;
    for (i = 0; assign[i] && name[i]; ++i) {
        c = (unsigned char)name[i];
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if (c != (unsigned char)assign[i])
            break;
    }
    if (!assign[i])
        relative += i;
    for (p = start = relative; ; ++p) {
        c = (unsigned char)*p;
        if (!c || c == '/') {
            if (p == start || p[-1] == '.' || p[-1] == ' ' ||
                device_name(start, p - start))
                return NULL;
            if (!c)
                return relative;
            start = p + 1;
        } else if (c < 32 || c > 126 || strchr("\\:\"<>|?*", c)) {
            return NULL;
        }
    }
}

void vc_init(struct vc_project *project)
{
    memset(project, 0, sizeof(*project));
    strcpy(project->author, "AUTHOR");
    project->track = 1;
}

unsigned vc_length(const struct vc_cue *cue)
{
    unsigned duration, step;
    if (cue->effect == VC_CUT)
        return 1;
    /* CalculateLength uses 2*a, unlike the actual field step. */
    duration = 2 * cue->a;
    step = (256 + duration - 1) / duration;
    return ((256 + step - 1) / step) / 2 + 1;
}

int vc_place(struct vc_project *project, unsigned index, const struct vc_cue *cue)
{
    struct vc_cue next = *cue;
    unsigned i, slot = 0, n = project->cues;
    unsigned long end;

    if (index > n || (index == n && n == VC_CUES) || cue->tick > VC_TICK_MAX ||
        (cue->effect != VC_CUT && cue->effect != VC_SLIDE) ||
        (cue->effect == VC_SLIDE && (!cue->a || cue->a > 255)))
        return 0;
    end = cue->tick + vc_length(cue);
    if (end > VC_TICK_MAX + 1)
        return 0;
    for (i = 0; i < n; ++i) {
        if (i == index)
            continue;
        if (project->cue[i].tick < end &&
            cue->tick < project->cue[i].tick + vc_length(&project->cue[i]))
            return 0;
        if (project->cue[i].tick < cue->tick)
            ++slot;
    }
    if (index < n) {
        --n;
        memmove(project->cue + index, project->cue + index + 1,
                (n - index) * sizeof(*cue));
    }
    memmove(project->cue + slot + 1, project->cue + slot, (n - slot) * sizeof(*cue));
    project->cue[slot] = next;
    project->cues = n + 1;
    return 1;
}

struct reader {
    const char *next;
    size_t left;
};

static int line(struct reader *reader, char *out, size_t room)
{
    const char *start = reader->next;
    const char *end = memchr(start, '\n', reader->left);
    size_t length;

    if (!end)
        return 0;
    length = end - start;
    if (length && end[-1] == '\r')
        --length;
    if (length >= room || memchr(start, 0, length) || memchr(start, '\r', length))
        return 0;
    memcpy(out, start, length);
    out[length] = 0;
    reader->left -= (end + 1) - start;
    reader->next = end + 1;
    return 1;
}

static int number(struct reader *reader, unsigned long limit, unsigned long *out)
{
    char token[16];
    const char *p;
    unsigned long value = 0;
    unsigned digit;

    if (!line(reader, token, sizeof(token)) || !*token)
        return 0;
    for (p = token; *p; ++p) {
        digit = (unsigned char)*p - (unsigned)'0';
        if (digit > 9 || digit > limit || value > (limit - digit) / 10)
            return 0;
        value = value * 10 + digit;
    }
    *out = value;
    return 1;
}

static int word(struct reader *reader, unsigned limit, unsigned *out)
{
    unsigned long value;
    if (!number(reader, limit, &value))
        return 0;
    *out = (unsigned)value;
    return 1;
}

static int unshift(char *token)
{
    unsigned char *p = (unsigned char *)token;
    if (!*p)
        return 0;
    for (; *p; ++p) {
        if (*p <= 32 || *p == 127)
            return 0;
        --*p;
    }
    return 1;
}

static int earlier_picture(const struct vc_project *project, unsigned next, const char *name)
{
    unsigned i;
    for (i = 0; i < next; ++i)
        if (!project->screen[i].second[0] && !strcmp(project->screen[i].first, name))
            return 1;
    return 0;
}

/* Parse into a candidate, never into the live project. Match the active
   writer's records, not the unrelated defvid grammar. */
const char *vc_read(struct vc_project *project, const char *data, size_t size)
{
    struct reader reader;
    struct vc_screen *screen;
    struct vc_cue cue = { 0 };
    char token[sizeof(header)];
    unsigned count, i, object;

    reader.next = data;
    reader.left = size;
    vc_init(project);
    if (!line(&reader, token, sizeof(token)) || strcmp(token, header))
        return "Not an active VC10 header.";
    if (!line(&reader, project->author, sizeof(project->author)) ||
        (unsigned char)project->author[0] <= 32 ||
        !word(&reader, 99, &project->track) ||
        !word(&reader, VC_SCREENS, &project->screens))
        return "Invalid author, track, or screen count.";
    for (i = 0; i < project->screens; ++i) {
        screen = &project->screen[i];
        if (!line(&reader, screen->first, sizeof(screen->first)) ||
            !strcmp(screen->first, "NOTEXT") || !unshift(screen->first) ||
            !vc_picture_path(screen->first) ||
            !line(&reader, screen->second, sizeof(screen->second)))
            return "Invalid picture reference.";
        if (!strcmp(screen->second, "ZIP")) {
            screen->second[0] = 0;
        } else if (!strcmp(screen->second, "NOTEXT") || !unshift(screen->second) ||
                   !vc_picture_path(screen->second) ||
                   !earlier_picture(project, i, screen->first) ||
                   !earlier_picture(project, i, screen->second)) {
            return "Two-picture screens must reference two earlier ordinary pictures.";
        }
    }
    for (i = 0; i < 3; ++i)
        if (!word(&reader, 0, &count))
            return "Vector, QuadAnim and 3D sections must be empty.";
    if (!word(&reader, VC_CUES, &count) || !count)
        return "Invalid or empty cue list.";
    for (i = 0; i < count; ++i) {
        if (!number(&reader, VC_TICK_MAX, &cue.tick) ||
            !word(&reader, VC_SLIDE, &cue.effect) || !cue.effect ||
            !word(&reader, project->screens + VC_FIRST_SCREEN - 1, &cue.screen) ||
            cue.screen < VC_FIRST_SCREEN ||
            !word(&reader, 65535, &cue.a) || !word(&reader, 65535, &cue.b) ||
            !word(&reader, 65535, &cue.c) || !word(&reader, 65535, &cue.d) ||
            !word(&reader, 65535, &cue.e) || !word(&reader, 0, &object) ||
            !line(&reader, cue.text, sizeof(cue.text)) ||
            (cue.effect == VC_SLIDE && (!cue.a || cue.a > 255)))
            return "Invalid or unsupported effect, screen, parameter, or object.";
        if (!strcmp(cue.text, "NOTEXT"))
            cue.text[0] = 0;
        else if (!unshift(cue.text))
            return "Invalid effect text.";
        if ((i && project->cue[i - 1].tick > cue.tick) ||
            !vc_place(project, i, &cue))
            return "Cues must be in order and must not overlap.";
    }
    return reader.left ? "Unexpected data after VC10 cue list." : NULL;
}

static char *shift(char *out, const char *text, const char *empty)
{
    if (!*text)
        return out + sprintf(out, "%s\n", empty);
    while (*text)
        *out++ = (char)((unsigned char)*text++ + 1);
    *out++ = '\n';
    return out;
}

/* The model limits bound the largest output below 24 KiB. */
size_t vc_write(const struct vc_project *project, char data[VC_BYTES])
{
    const struct vc_cue *cue;
    unsigned i;
    char *out = data;

    out += sprintf(out, "%s\n%s\n%u\n%u\n", header, project->author,
                   project->track, project->screens);
    for (i = 0; i < project->screens; ++i) {
        out = shift(out, project->screen[i].first, "NOTEXT");
        out = shift(out, project->screen[i].second, "ZIP");
    }
    out += sprintf(out, "0\n0\n0\n%u\n", project->cues);
    for (i = 0; i < project->cues; ++i) {
        cue = &project->cue[i];
        out += sprintf(out, "%lu\n%u\n%u\n%u\n%u\n%u\n%u\n%u\n0\n",
                       cue->tick, cue->effect, cue->screen, cue->a,
                       cue->b, cue->c, cue->d, cue->e);
        out = shift(out, cue->text, "NOTEXT");
    }
    return out - data;
}

struct vc_frame vc_at(const struct vc_project *project, unsigned long tick)
{
    struct vc_frame frame = { 0, 0, VC_HEIGHT };
    const struct vc_cue *cue;
    unsigned i, step;
    unsigned long distance;

    for (i = 0; i < project->cues && project->cue[i].tick <= tick; ++i) {
        cue = &project->cue[i];
        if (cue->effect == VC_SLIDE && tick - cue->tick < 128) {
            step = (256 + (2 * cue->a - 1) - 1) / (2 * cue->a - 1);
            /* MainLoop starts at field 2*Time; DoDisplay steps before
               rendering. Sample after both logical fields of this tick. */
            distance = (2 * (tick - cue->tick) + 2) * step;
            if (distance < 255) {
                frame.split = 255 - (unsigned)distance;
                frame.bottom = cue->screen;
                return frame;
            }
        }
        frame.top = cue->screen;
    }
    return frame;
}

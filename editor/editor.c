/* Milestone 3 Win32 controls, local files, clock and indexed DIB. */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "ilbm.h"
#include "local.h"
#include "vc10.h"

#define TITLE "AmigaVid VCEDIT"
#define CLASS "AmigaVidEditor"
#define WIDTH 592
#define HEIGHT 438
#define GRID_X 16
#define GRID_Y 370
#define GRID_W 552
#define GRID_BOTTOM 419

enum {
    ADD_PICTURE = 100, OPEN_PROJECT, SAVE_PROJECT, ADD_CUT, ADD_SLIDE,
    PLAY, STOP, SEEK, PICTURES, NAME, DURATION, TICK,
    ADD_SLIDE_ON_DOWN, ADD_SLIDE_OFF_UP, ADD_SLIDE_OFF_DOWN,
    ADD_SCROLL_UP, ADD_SCROLL_DOWN, ADD_REVEAL_UP, ADD_REVEAL_DOWN,
    ADD_PEEL_UP, ADD_PEEL_DOWN, ADD_SQUEEZE_ON_UP, ADD_SQUEEZE_ON_DOWN,
    ADD_SQUEEZE_OFF_UP, ADD_SQUEEZE_OFF_DOWN
};

static struct document {
    struct vc_project project;
    struct ilbm_image picture[VC_SCREENS];
    unsigned first[VC_SCREENS], pictures;
    char name[VC_NAME];
} *doc;

static HWND window, files, pictures, duration, position, status;
static unsigned long tick, shown_tick;
static DWORD started;
static int playing, dragging = -1, drag_x, dirty;
static int selected = -1, drag_start_x, drag_start_y, drag_offset, drag_moved;
static int painted;
static struct vc_frame shown;
static unsigned char rgb[VC_WIDTH * VC_HEIGHT * 3], indexed[VC_WIDTH * VC_HEIGHT];
static struct {
    BITMAPINFOHEADER header;
    WORD colors[256];
} dib;
static BITMAPINFO truecolor;
static HPALETTE palette;

static const char *effect_name(unsigned effect)
{
    switch (effect) {
    case VC_CUT: return "Straight Cut";
    case VC_SLIDE: return "Slide On Up";
    case VC_SLIDE_ON_DOWN: return "Slide On Down";
    case VC_SLIDE_OFF_UP: return "Slide Off Up";
    case VC_SLIDE_OFF_DOWN: return "Slide Off Down";
    case VC_SCROLL_UP: return "Scroll Up";
    case VC_SCROLL_DOWN: return "Scroll Down";
    case VC_REVEAL_UP: return "Reveal Up";
    case VC_REVEAL_DOWN: return "Reveal Down";
    case VC_PEEL_UP: return "Peel Up";
    case VC_PEEL_DOWN: return "Peel Down";
    case VC_SQUEEZE_ON_UP: return "Squeeze On Up";
    case VC_SQUEEZE_ON_DOWN: return "Squeeze On Down";
    case VC_SQUEEZE_OFF_UP: return "Squeeze Off Up";
    case VC_SQUEEZE_OFF_DOWN: return "Squeeze Off Down";
    }
    return "Unsupported";
}

static void message(const char *text)
{
    SetWindowTextA(status, text);
}

static void error(const char *text)
{
    MessageBoxA(window, text, TITLE, MB_OK | MB_ICONERROR);
}

static void free_document(struct document *d)
{
    unsigned i;
    for (i = 0; i < d->pictures; ++i)
        ilbm_free(&d->picture[i]);
    free(d);
}

static const char *add_image(struct document *d, const char *reference)
{
    char *data;
    DWORD size;
    struct ilbm_image *image = &d->picture[d->pictures];
    const char *problem = local_read(vc_picture_path(reference), ILBM_MAX_FILE, &data, &size);

    if (!problem)
        problem = ilbm_decode((const unsigned char *)data, size, image);
    free(data);
    if (problem)
        return problem;
    if (image->width > VC_WIDTH || image->height > VC_HEIGHT) {
        ilbm_free(image);
        return "The editor supports pictures up to 320 x 256.";
    }
    ++d->pictures;
    return NULL;
}

static unsigned prior_image(const struct document *d, unsigned screen, const char *name)
{
    unsigned i;
    for (i = 0; i < screen; ++i)
        if (!d->project.screen[i].second[0] && !strcmp(name, d->project.screen[i].first))
            return d->first[i];
    return d->pictures;   /* vc_read has already checked these references. */
}

static void row(unsigned char *out, unsigned screen, unsigned y)
{
    const struct ilbm_image *image;
    unsigned margin, x;
    const unsigned char *in;

    memset(out, 0, VC_WIDTH * 3);
    if (!screen)
        return;
    image = &doc->picture[doc->first[screen - VC_FIRST_SCREEN]];
    margin = (VC_HEIGHT - image->height) / 2;
    if (y < margin || y >= margin + image->height)
        return;
    in = image->rgb + (y - margin) * image->width * 3;
    out += (VC_WIDTH - image->width) / 2 * 3;
    for (x = 0; x < image->width; ++x, in += 3, out += 3) {
        out[0] = in[2];
        out[1] = in[1];
        out[2] = in[0];
    }
}

static HPALETTE make_palette(void)
{
    struct {
        WORD version, count;
        PALETTEENTRY entry[256];
    } logical;
    unsigned short table[512] = { 0 };
    unsigned long key;
    unsigned i, k, n;
    const unsigned char *in = rgb;
    PALETTEENTRY *entry;

    logical.version = 0x300;
    logical.count = 0;
    /* The two simultaneous pictures can have different palettes. Match exact
       RGB while they fit one palette; otherwise use the RGB DIB. */
    for (i = 0; i < sizeof(indexed); ++i, in += 3) {
        key = ((unsigned long)in[2] << 16) | ((unsigned long)in[1] << 8) | in[0];
        k = (key * 2654435761UL) >> 23;
        while (table[k]) {
            entry = &logical.entry[table[k] - 1];
            if (entry->peRed == in[2] && entry->peGreen == in[1] && entry->peBlue == in[0])
                break;
            k = (k + 1) & 511;
        }
        if (!table[k]) {
            n = logical.count;
            if (n == 256)
                return NULL;
            logical.entry[n].peRed = in[2];
            logical.entry[n].peGreen = in[1];
            logical.entry[n].peBlue = in[0];
            logical.entry[n].peFlags = PC_NOCOLLAPSE;
            table[k] = ++logical.count;
            dib.colors[n] = (WORD)n;
        }
        indexed[i] = (unsigned char)(table[k] - 1);
    }
    dib.header.biClrUsed = logical.count;
    return CreatePalette((const LOGPALETTE *)&logical);
}

static void refresh(void)
{
    struct vc_frame frame = vc_at(&doc->project, tick);
    unsigned y, source;
    char label[64];

    if (!painted || shown.top != frame.top || shown.bottom != frame.bottom ||
        shown.split != frame.split || shown.top_row != frame.top_row ||
        shown.bottom_row != frame.bottom_row || shown.reflect != frame.reflect ||
        shown.top_step != frame.top_step || shown.bottom_step != frame.bottom_step) {
        for (y = 0; y < frame.split; ++y) {
            source = y > frame.reflect ? 2 * frame.reflect - y : y;
            row(rgb + y * VC_WIDTH * 3, frame.top,
                frame.top_row + source * frame.top_step / VC_ROW_ONE);
        }
        for (; y < VC_HEIGHT; ++y)
            row(rgb + y * VC_WIDTH * 3, frame.bottom,
                frame.bottom_row + (y - frame.split) * frame.bottom_step / VC_ROW_ONE);
        if (palette)
            DeleteObject(palette);
        palette = make_palette();
        shown = frame;
        painted = 1;
    }
    if (shown_tick != tick) {
        wsprintfA(label, "%lu", tick);
        SetWindowTextA(position, label);
        shown_tick = tick;
    }
    InvalidateRect(window, NULL, FALSE);
}

static void stop(void)
{
    if (playing)
        KillTimer(window, 1);
    playing = 0;
}

static unsigned long end_tick(void)
{
    const struct vc_project *p = &doc->project;
    const struct vc_cue *last;
    if (!p->cues)
        return 0;
    last = &p->cue[p->cues - 1];
    return last->tick + vc_length(last) - 1;
}

static unsigned long visible_ticks(void)
{
    unsigned long end = end_tick();
    return end < 124 ? 124 : end < VC_TICK_MAX - 25 ? end + 25 : VC_TICK_MAX;
}

static int to_x(unsigned long time)
{
    unsigned long span = visible_ticks();
    return GRID_X + (int)((time < span ? time : span) * GRID_W / span);
}

static unsigned long to_tick(int x)
{
    if (x <= GRID_X)
        return 0;
    if (x >= GRID_X + GRID_W)
        return visible_ticks();
    return ((unsigned long)(x - GRID_X) * visible_ticks() + GRID_W / 2) / GRID_W;
}

static RECT marker_rect(const struct vc_cue *cue)
{
    RECT marker;

    marker.left = to_x(cue->tick);
    marker.right = to_x(cue->tick + vc_length(cue));
    if (marker.right < marker.left + 6)
        marker.right = marker.left + 6;
    marker.top = GRID_Y + 1;
    marker.bottom = GRID_Y + 9;
    return marker;
}

static int hit_cue(int x, int y)
{
    const struct vc_project *p = &doc->project;
    POINT point;
    RECT marker;
    int i;

    point.x = x;
    point.y = y;
    /* Paint in timeline order; hit the topmost body, then enter its group. */
    for (i = (int)p->cues - 1; i >= 0; --i) {
        marker = marker_rect(&p->cue[i]);
        if (PtInRect(&marker, point)) {
            while (i > 0 && to_x(p->cue[i - 1].tick) == marker.left)
                --i;
            return i;
        }
    }
    for (i = 0; i < (int)p->cues; ++i)
        if (abs(x - to_x(p->cue[i].tick)) <= 5)
            return i;
    return -1;
}

static void list_picture(unsigned index)
{
    char label[80];
    struct vc_screen *screen = &doc->project.screen[index];
    wsprintfA(label, "%u: %s%s%s", index + VC_FIRST_SCREEN,
              vc_picture_path(screen->first), screen->second[0] ? " + " : "",
              screen->second[0] ? vc_picture_path(screen->second) : "");
    SendMessageA(pictures, CB_ADDSTRING, 0, (LPARAM)label);
    SendMessageA(pictures, CB_SETCURSEL, index, 0);
}

static int leaf_name(char name[VC_NAME], int project)
{
    unsigned n;
    if (GetWindowTextLengthA(files) >= VC_NAME)
        return 0;
    GetWindowTextA(files, name, VC_NAME);
    n = (unsigned)strlen(name);
    return vc_picture_path(name) == name && !strchr(name, '/') &&
           (!project || (n > 4 && lstrcmpiA(name + n - 4, ".vid") == 0));
}

static int discard_changes(void)
{
    return !dirty || MessageBoxA(window, "Discard unsaved changes?", TITLE,
                                 MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES;
}

static void open_project(void)
{
    struct document *candidate;
    char name[VC_NAME], *data;
    DWORD size;
    unsigned i, paired = 0;
    const char *problem;

    if (!leaf_name(name, 1)) {
        error("Enter a local .vid filename.");
        return;
    }
    candidate = calloc(1, sizeof(*candidate));
    if (!candidate) {
        error("Not enough memory for a project.");
        return;
    }
    problem = local_read(name, VC_BYTES, &data, &size);
    if (!problem)
        problem = vc_read(&candidate->project, data, size);
    free(data);
    for (i = 0; !problem && i < candidate->project.screens; ++i) {
        struct vc_screen *screen = &candidate->project.screen[i];
        if (*screen->second) {
            candidate->first[i] = prior_image(candidate, i, screen->first);
            ++paired;
        } else {
            candidate->first[i] = candidate->pictures;
            problem = add_image(candidate, screen->first);
        }
    }
    if (problem || !discard_changes()) {
        free_document(candidate);
        if (problem)
            error(problem);
        return;
    }
    dragging = selected = -1;
    if (GetCapture() == window)
        ReleaseCapture();
    free_document(doc);
    doc = candidate;
    strcpy(doc->name, name);
    SendMessageA(pictures, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < doc->project.screens; ++i)
        list_picture(i);
    tick = 0;
    painted = dirty = 0;
    message(paired ? "Project opened. Two-picture screens preview their first image." : "Project opened.");
    refresh();
}

static void save_project(void)
{
    char name[VC_NAME], *data;
    const char *notice;
    int published;
    if (!doc->project.cues) {
        error("Add a cue before saving.");
        return;
    }
    if (!leaf_name(name, 1)) {
        error("Enter a local .vid filename.");
        return;
    }
    if (!(data = malloc(VC_BYTES))) {
        error("Not enough memory to stage project.");
        return;
    }
    notice = local_save(name, data, (DWORD)vc_write(&doc->project, data),
                        lstrcmpiA(doc->name, name) == 0, &published);
    free(data);
    if (!published) {
        error(notice);
        return;
    }
    strcpy(doc->name, name);
    dirty = 0;
    message(notice ? "Project saved; .bak retained." : "Project saved.");
    if (notice)
        MessageBoxA(window, notice, TITLE, MB_OK | MB_ICONWARNING);
}

static void open_picture(void)
{
    char name[VC_NAME], reference[VC_NAME];
    const char *problem;
    unsigned n = doc->project.screens;

    if (!leaf_name(name, 0) || strlen(name) + sizeof("VCPics:") > VC_NAME) {
        error("Enter a local picture filename (24 ASCII characters maximum).");
        return;
    }
    if (n == VC_SCREENS) {
        error("The screen list is full.");
        return;
    }
    wsprintfA(reference, "VCPics:%s", name);
    problem = add_image(doc, reference);
    if (problem) {
        error(problem);
        return;
    }
    strcpy(doc->project.screen[n].first, reference);
    doc->first[n] = doc->pictures - 1;
    ++doc->project.screens;
    dirty = 1;
    list_picture(n);
    message("Picture opened. Select it and add an effect.");
}

static void add_cue(unsigned effect)
{
    struct vc_cue cue = { 0 };
    struct vc_project *p = &doc->project;
    char text[4];
    LRESULT choice = SendMessageA(pictures, CB_GETCURSEL, 0, 0);

    if (choice == CB_ERR) {
        error("Open and select a picture first.");
        return;
    }
    cue.screen = (unsigned)choice + VC_FIRST_SCREEN;
    cue.effect = effect;
    if (effect != VC_CUT) {
        if (GetWindowTextLengthA(duration) >= sizeof(text) ||
            !GetWindowTextA(duration, text, sizeof(text)) ||
            strspn(text, "0123456789") != strlen(text) ||
            !(cue.a = (unsigned)atoi(text)) || cue.a > 255) {
            error("Enter an effect duration of 1 to 255 ticks.");
            return;
        }
    }
    cue.tick = p->cues ? end_tick() + 25 : 0;
    if (!vc_place(p, p->cues, &cue)) {
        error("No room for another cue.");
        return;
    }
    dirty = 1;
    selected = -1;
    message("Cue added. Drag its timeline marker to change the start tick.");
    refresh();
}

static void text_at(HDC dc, int x, int y, const char *text)
{
    TextOutA(dc, x, y, text, (int)strlen(text));
}

static void draw_timeline(HDC dc)
{
    RECT area = { 0, 323, WIDTH, HEIGHT };
    RECT marker;
    const struct vc_cue *cue;
    unsigned i;
    int x;
    char label[96];

    FillRect(dc, &area, (HBRUSH)(COLOR_WINDOW + 1));
    SetBkMode(dc, TRANSPARENT);
    wsprintfA(label, "Tick %lu   |   25 ticks/sec   |   %u cues   |   Drag markers; click track to seek",
              tick, doc->project.cues);
    text_at(dc, GRID_X, 338, label);
    MoveToEx(dc, GRID_X, GRID_Y, NULL);
    LineTo(dc, GRID_X + GRID_W, GRID_Y);
    for (i = 0; i < doc->project.cues; ++i) {
        cue = &doc->project.cue[i];
        marker = marker_rect(cue);
        Rectangle(dc, marker.left, marker.top, marker.right, marker.bottom);
        wsprintfA(label, "%lu %s #%u", cue->tick, effect_name(cue->effect), cue->screen);
        text_at(dc, marker.left, GRID_Y + 11 + 16 * (i % 2), label);
    }
    x = dragging >= 0 ? drag_x : to_x(tick);
    MoveToEx(dc, x, GRID_Y - 9, NULL);
    LineTo(dc, x, GRID_Y + 10);
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT paint;
    HDC dc;
    HPALETTE old;
    unsigned i, changed;
    unsigned long next;
    struct vc_cue cue;
    int x, y;
    char label[64];
    BOOL ok;

    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ADD_PICTURE: stop(); open_picture(); return 0;
        case OPEN_PROJECT: stop(); open_project(); return 0;
        case SAVE_PROJECT: stop(); save_project(); return 0;
        case ADD_CUT: stop(); add_cue(VC_CUT); return 0;
        case ADD_SLIDE: stop(); add_cue(VC_SLIDE); return 0;
        case ADD_SLIDE_ON_DOWN: stop(); add_cue(VC_SLIDE_ON_DOWN); return 0;
        case ADD_SLIDE_OFF_UP: stop(); add_cue(VC_SLIDE_OFF_UP); return 0;
        case ADD_SLIDE_OFF_DOWN: stop(); add_cue(VC_SLIDE_OFF_DOWN); return 0;
        case ADD_SCROLL_UP: stop(); add_cue(VC_SCROLL_UP); return 0;
        case ADD_SCROLL_DOWN: stop(); add_cue(VC_SCROLL_DOWN); return 0;
        case ADD_REVEAL_UP: stop(); add_cue(VC_REVEAL_UP); return 0;
        case ADD_REVEAL_DOWN: stop(); add_cue(VC_REVEAL_DOWN); return 0;
        case ADD_PEEL_UP: stop(); add_cue(VC_PEEL_UP); return 0;
        case ADD_PEEL_DOWN: stop(); add_cue(VC_PEEL_DOWN); return 0;
        case ADD_SQUEEZE_ON_UP: stop(); add_cue(VC_SQUEEZE_ON_UP); return 0;
        case ADD_SQUEEZE_ON_DOWN: stop(); add_cue(VC_SQUEEZE_ON_DOWN); return 0;
        case ADD_SQUEEZE_OFF_UP: stop(); add_cue(VC_SQUEEZE_OFF_UP); return 0;
        case ADD_SQUEEZE_OFF_DOWN: stop(); add_cue(VC_SQUEEZE_OFF_DOWN); return 0;
        case PLAY:
            if (!doc->project.cues)
                return 0;
            stop();
            started = GetTickCount();
            if (!SetTimer(hwnd, 1, VC_TICK_MS, NULL)) {
                error("Cannot start the playback clock.");
                return 0;
            }
            playing = 1;
            tick = 0;
            message("Playing.");
            refresh();
            return 0;
        case STOP: stop(); message("Stopped."); return 0;
        case SEEK:
            next = GetDlgItemInt(hwnd, TICK, &ok, FALSE);
            if (ok && next <= VC_TICK_MAX) {
                stop();
                tick = next;
                shown_tick = VC_TICK_MAX + 1;
                refresh();
            } else {
                error("Enter a tick from 0 to 65535.");
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if (!playing)
            return 0;
        next = (GetTickCount() - started) / VC_TICK_MS;
        if (next >= end_tick()) {
            next = end_tick();
            stop();
            message("Playback complete.");
        }
        tick = next;
        refresh();
        return 0;
    case WM_LBUTTONDOWN:
        x = (short)LOWORD(lp);
        y = (short)HIWORD(lp);
        if (y < GRID_Y - 9 || y > GRID_BOTTOM)
            return 0;
        stop();
        dragging = hit_cue(x, y);
        if (dragging >= 0) {
            if (selected >= 0 && (unsigned)selected < doc->project.cues &&
                to_x(doc->project.cue[selected].tick) == to_x(doc->project.cue[dragging].tick))
                dragging = selected;
            drag_x = to_x(doc->project.cue[dragging].tick);
            drag_start_x = x;
            drag_start_y = y;
            drag_offset = x - drag_x;
            drag_moved = 0;
            SetCapture(hwnd);
            return 0;
        }
        selected = -1;
        tick = to_tick(x);
        message("Timeline seek.");
        refresh();
        return 0;
    case WM_MOUSEMOVE:
        if (dragging >= 0) {
            x = (short)LOWORD(lp);
            y = (short)HIWORD(lp);
            if (x != drag_start_x || y != drag_start_y)
                drag_moved = 1;
            drag_x = to_x(to_tick(x - drag_offset));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        dragging = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (dragging >= 0) {
            i = (unsigned)dragging;
            dragging = -1;
            ReleaseCapture();
            if (i >= doc->project.cues) {
                selected = -1;
                return 0;
            }
            cue = doc->project.cue[i];
            x = (short)LOWORD(lp);
            y = (short)HIWORD(lp);
            if (!drag_moved && x == drag_start_x && y == drag_start_y) {
                /* Cycle only on release, so a drag keeps the selected cue. */
                if (selected == (int)i) {
                    changed = (i + 1) % doc->project.cues;
                    while (changed != i &&
                           to_x(doc->project.cue[changed].tick) != to_x(cue.tick))
                        changed = (changed + 1) % doc->project.cues;
                    i = changed;
                    cue = doc->project.cue[i];
                }
                next = cue.tick;
            } else {
                next = abs(x - drag_start_x) <= 2 ? cue.tick : to_tick(x - drag_offset);
            }
            if (next != cue.tick) {
                cue.tick = next;
                if (vc_place(&doc->project, i, &cue)) {
                    dirty = 1;
                    message("Cue moved.");
                    for (i = 0; i < doc->project.cues; ++i)
                        if (doc->project.cue[i].tick == next)
                            break;
                } else {
                    message("Cue overlaps another effect or the end of the timeline.");
                    next = doc->project.cue[i].tick;
                }
            } else {
                wsprintfA(label, "Selected cue at tick %lu.", next);
                message(label);
            }
            selected = (int)i;
            tick = next;
            shown_tick = VC_TICK_MAX + 1;  /* Discard unsubmitted text on selection. */
            refresh();
        }
        return 0;
    case WM_PAINT:
        dc = BeginPaint(hwnd, &paint);
        if (palette) {
            old = SelectPalette(dc, palette, FALSE);
            RealizePalette(dc);
            StretchDIBits(dc, 0, 0, VC_WIDTH, VC_HEIGHT, 0, 0, VC_WIDTH, VC_HEIGHT,
                          indexed, (const BITMAPINFO *)&dib, DIB_PAL_COLORS, SRCCOPY);
            SelectPalette(dc, old, TRUE);
        } else {
            StretchDIBits(dc, 0, 0, VC_WIDTH, VC_HEIGHT, 0, 0, VC_WIDTH, VC_HEIGHT,
                          rgb, &truecolor, DIB_RGB_COLORS, SRCCOPY);
        }
        draw_timeline(dc);
        EndPaint(hwnd, &paint);
        return 0;
    case WM_QUERYNEWPALETTE:
    case WM_PALETTECHANGED:
        if (!palette || (msg == WM_PALETTECHANGED && (HWND)wp == hwnd))
            return 0;
        dc = GetDC(hwnd);
        old = SelectPalette(dc, palette, msg == WM_PALETTECHANGED);
        changed = RealizePalette(dc);
        SelectPalette(dc, old, TRUE);
        ReleaseDC(hwnd, dc);
        if (changed)
            InvalidateRect(hwnd, NULL, FALSE);
        return changed != 0;
    case WM_CLOSE:
        stop();
        if (discard_changes())
            DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HWND control(const char *type, const char *label, DWORD style,
                    int x, int y, int w, int h, int id)
{
    HWND child = CreateWindowExA(0, type, label, WS_CHILD | WS_VISIBLE | style,
                                 x, y, w, h, window, (HMENU)id, NULL, NULL);
    if (child)
        SendMessageA(child, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
    return child;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    WNDCLASSA cls = { 0 };
    RECT rect = { 0, 0, WIDTH, HEIGHT };
    MSG msg;
    HACCEL keys;
    ACCEL commands[] = {
        { FVIRTKEY | FCONTROL, 'I', ADD_PICTURE }, { FVIRTKEY | FCONTROL, 'O', OPEN_PROJECT },
        { FVIRTKEY | FCONTROL, 'S', SAVE_PROJECT }, { FVIRTKEY, VK_F5, PLAY },
        { FVIRTKEY, VK_ESCAPE, STOP }
    };
    const char *problem;
    int result;
    (void)previous;

    if ((problem = local_init(args))) {
        error(problem);
        return 2;
    }
    doc = calloc(1, sizeof(*doc));
    if (!doc) {
        error("Not enough memory to initialize the editor.");
        local_close();
        return 1;
    }
    vc_init(&doc->project);
    truecolor.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    truecolor.bmiHeader.biWidth = VC_WIDTH;
    truecolor.bmiHeader.biHeight = -VC_HEIGHT;
    truecolor.bmiHeader.biPlanes = 1;
    truecolor.bmiHeader.biBitCount = 24;
    truecolor.bmiHeader.biCompression = BI_RGB;
    dib.header = truecolor.bmiHeader;
    dib.header.biBitCount = 8;
    cls.lpfnWndProc = proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorA(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpszClassName = CLASS;
    AdjustWindowRect(&rect, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    if (RegisterClassA(&cls))
        window = CreateWindowExA(WS_EX_CONTROLPARENT, CLASS, TITLE,
                                 WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                 12, 12, rect.right - rect.left, rect.bottom - rect.top,
                                 NULL, NULL, instance, NULL);
    keys = CreateAcceleratorTableA(commands, sizeof(commands) / sizeof(commands[0]));
    result = 1;
    if (!window || !keys ||
        !control("STATIC", "Local filename (picture or .vid project)", 0, 332, 5, 252, 18, 0) ||
        !(files = control("EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                          332, 24, 250, 22, NAME)) ||
        !control("BUTTON", "Open p&icture", WS_TABSTOP, 332, 50, 80, 23, ADD_PICTURE) ||
        !control("BUTTON", "&Open VID", WS_TABSTOP, 416, 50, 80, 23, OPEN_PROJECT) ||
        !control("BUTTON", "&Save VID", WS_TABSTOP, 501, 50, 81, 23, SAVE_PROJECT) ||
        !control("STATIC", "Screen for the next cue", 0, 332, 82, 250, 18, 0) ||
        !(pictures = control("COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                             332, 101, 250, 96, PICTURES)) ||
        !control("BUTTON", "Add Straight &Cut", WS_TABSTOP, 332, 130, 118, 23, ADD_CUT) ||
        !control("BUTTON", "Add Slide On &Up", WS_TABSTOP, 464, 130, 118, 23, ADD_SLIDE) ||
        !control("BUTTON", "Add Slide On &Down", WS_TABSTOP, 332, 157, 118, 23, ADD_SLIDE_ON_DOWN) ||
        !control("BUTTON", "Add Slide O&ff Up", WS_TABSTOP, 464, 157, 118, 23, ADD_SLIDE_OFF_UP) ||
        !control("BUTTON", "Add Slide Off Do&wn", WS_TABSTOP, 332, 184, 118, 23, ADD_SLIDE_OFF_DOWN) ||
        !control("STATIC", "Ticks", 0, 464, 187, 42, 18, 0) ||
        /* A single-line EDIT can discard everything after a pasted line break. */
        !(duration = control("EDIT", "25", WS_BORDER | WS_TABSTOP | ES_NUMBER |
                             ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
                             510, 184, 72, 22, DURATION)) ||
        !control("BUTTON", "Add Sc&roll Up", WS_TABSTOP, 332, 211, 118, 23, ADD_SCROLL_UP) ||
        !control("BUTTON", "Add Scroll Dow&n", WS_TABSTOP, 464, 211, 118, 23, ADD_SCROLL_DOWN) ||
        !control("BUTTON", "Add Re&veal Up", WS_TABSTOP, 332, 238, 118, 23, ADD_REVEAL_UP) ||
        !control("BUTTON", "Add Revea&l Down", WS_TABSTOP, 464, 238, 118, 23, ADD_REVEAL_DOWN) ||
        !control("BUTTON", "Add P&eel Up", WS_TABSTOP, 332, 265, 118, 23, ADD_PEEL_UP) ||
        !control("BUTTON", "&Add Peel Down", WS_TABSTOP, 464, 265, 118, 23, ADD_PEEL_DOWN) ||
        !control("BUTTON", "Add Squeeze On Up", WS_TABSTOP, 8, 257, 146, 18, ADD_SQUEEZE_ON_UP) ||
        !control("BUTTON", "Add Squeeze On Down", WS_TABSTOP, 166, 257, 146, 18, ADD_SQUEEZE_ON_DOWN) ||
        !control("BUTTON", "Add Squeeze Off Up", WS_TABSTOP, 8, 276, 146, 18, ADD_SQUEEZE_OFF_UP) ||
        !control("BUTTON", "Add Squeeze Off Down", WS_TABSTOP, 166, 276, 146, 18, ADD_SQUEEZE_OFF_DOWN) ||
        !control("BUTTON", "&Play", WS_TABSTOP, 332, 292, 65, 23, PLAY) ||
        !control("BUTTON", "S&top", WS_TABSTOP, 401, 292, 65, 23, STOP) ||
        !(position = control("EDIT", "0", WS_BORDER | WS_TABSTOP | ES_NUMBER,
                             475, 292, 54, 22, TICK)) ||
        !control("BUTTON", "See&k", WS_TABSTOP, 533, 292, 49, 23, SEEK) ||
        !(status = control("STATIC", "Enter a filename from the workspace.", 0,
                           16, 296, 304, 27, 0))) {
        error("Cannot initialize editor controls.");
        goto done;
    }
    SendMessageA(files, EM_LIMITTEXT, MAX_PATH - 1, 0);
    /* Keep invalid input, not a truncated valid prefix, for add_cue to reject. */
    SendMessageA(duration, EM_LIMITTEXT, 0, 0);
    SendMessageA(position, EM_LIMITTEXT, 5, 0);
    refresh();
    ShowWindow(window, show);
    SetFocus(files);
    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        /* The multiline Ticks edit claims Tab. Keep it multiline for paste
           validation, but move to the next or previous dialog control. */
        if (msg.hwnd == duration && msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) {
            SetFocus(GetNextDlgTabItem(window, duration, GetKeyState(VK_SHIFT) < 0));
        } else if (!TranslateAcceleratorA(window, keys, &msg) && !IsDialogMessageA(window, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    result = result < 0 ? 1 : (int)msg.wParam;

done:
    if (keys)
        DestroyAcceleratorTable(keys);
    if (palette)
        DeleteObject(palette);
    free_document(doc);
    local_close();
    return result;
}

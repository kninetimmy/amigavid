/* Standalone ANSI Win32 compatibility probe; no original Video Creator assets. */
#include <windows.h>
#include <mmsystem.h>
#include <string.h>

#define TITLE "AmigaVid Win32 Probe"
#define CLASS_NAME "AmigaVidProbe"
#define DATA_NAME "VCPROBE.DAT"
#define SIDE 8
#define PIXELS (SIDE * SIDE)

static const struct {
    BITMAPINFOHEADER header;
    RGBQUAD colors[4];
} dib = {
    { sizeof(BITMAPINFOHEADER), SIDE, SIDE, 1, 8, BI_RGB, PIXELS, 0, 0, 4, 0 },
    { { 0, 0, 255, 0 }, { 0, 255, 0, 0 }, { 255, 0, 0, 0 }, { 255, 255, 255, 0 } }
};
static BYTE pixels[PIXELS];
static char audio_status[80];

static int fail(const char *message, int code)
{
    MessageBoxA(NULL, message, TITLE, MB_OK | MB_ICONERROR);
    return code;
}

static int read_picture(void)
{
    char path[MAX_PATH];
    char *name = NULL;
    char *p;
    DWORD length, size, error;
    BOOL read_ok;
    HANDLE file;
    BYTE data[4 + PIXELS + 1];
    int i;

    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (length == 0 || length >= sizeof(path))
        return fail("Cannot resolve the executable folder (ANSI MAX_PATH).", 2);
    for (p = path; *p; p = CharNextA(p)) {
        if (*p == '\\')
            name = p + 1;
    }
    if (!name || (name - path) + sizeof(DATA_NAME) > sizeof(path))
        return fail("The executable folder is too long for " DATA_NAME ".", 2);
    memcpy(name, DATA_NAME, sizeof(DATA_NAME));

    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return fail("Missing " DATA_NAME ". Extract it next to VCPROBE.EXE.", 2);
        return fail("Cannot open " DATA_NAME " next to VCPROBE.EXE.", 2);
    }
    read_ok = ReadFile(file, data, sizeof(data), &size, NULL);
    CloseHandle(file);
    if (!read_ok)
        return fail("Cannot read " DATA_NAME " next to VCPROBE.EXE.", 2);

    /* AVP1 plus exactly 64 palette indices, in top-to-bottom order. */
    if (size == 4 + PIXELS && memcmp(data, "AVP1", 4) == 0) {
        for (i = 0; i < PIXELS && data[4 + i] < 4; ++i)
            pixels[(SIDE - 1 - i / SIDE) * SIDE + i % SIDE] = data[4 + i];
        if (i == PIXELS)
            return 0;
    }
    return fail("Invalid " DATA_NAME ". Re-extract it from the probe ZIP.", 3);
}

static void show_status(HWND window, const char *input)
{
    char caption[128];
    wsprintfA(caption, TITLE " - %s - %s", input, audio_status);
    SetWindowTextA(window, caption);
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    static const char hint[] = "Click the picture; press Space; press Esc or close X.";
    PAINTSTRUCT paint;
    HDC dc;

    switch (msg) {
    case WM_PAINT:
        dc = BeginPaint(window, &paint);
        StretchDIBits(dc, 12, 12, 128, 128, 0, 0, SIDE, SIDE, pixels,
                      (const BITMAPINFO *)&dib, DIB_RGB_COLORS, SRCCOPY);
        TextOutA(dc, 12, 154, hint, sizeof(hint) - 1);
        EndPaint(window, &paint);
        return 0;
    case WM_LBUTTONDOWN:
        show_status(window, "Mouse");
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            DestroyWindow(window);
            return 0;
        }
        if (wp == VK_SPACE) {
            show_status(window, "Space");
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    WNDCLASSA wc = { 0 };
    MSG msg;
    HWND window;
    UINT devices;
    BOOL simulated;
    int status;

    (void)previous;
    status = read_picture();
    if (status != 0)
        return status;

    devices = waveOutGetNumDevs();
    /* Query only; never open playback devices, even when present. */
    simulated = strcmp(args, "/noaudio") == 0;
    if (devices == 0 || simulated)
        wsprintfA(audio_status, "No audio (%sWinMM waveOut=%u)",
                  simulated ? "simulated; " : "", devices);
    else
        wsprintfA(audio_status, "WinMM waveOut=%u", devices);

    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassA(&wc))
        return fail("Cannot register the probe window.", 1);

    window = CreateWindowExA(0, CLASS_NAME, TITLE, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 550, 245,
                             NULL, NULL, instance, NULL);
    if (!window)
        return fail("Cannot create the probe window.", 1);
    show_status(window, "Ready");
    ShowWindow(window, show);
    UpdateWindow(window);

    while ((status = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (status < 0)
        return fail("Cannot read the Windows message queue.", 1);
    return (int)msg.wParam;
}

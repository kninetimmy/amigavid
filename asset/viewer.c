/* Native milestone-2 viewer. Only these two leaf names can reach the loader. */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "ilbm.h"

#define TITLE "AmigaVid ILBM Viewer"
#define CLASS_NAME "AmigaVidAsset"
#define OPEN_BACKGROUND 100
#define OPEN_PICTURE 101

static const char *const names[] = { "blankeditor.iff", "blueface.16" };
static char root[MAX_PATH];  /* Absolute drive path, with a trailing backslash. */
static int win9x;
static BITMAPINFO dib;
static unsigned char *pixels;

static void error_box(HWND window, const char *name, const char *reason)
{
    char message[256];
    wsprintfA(message, "%s: %s", name, reason);
    MessageBoxA(window, message, TITLE, MB_OK | MB_ICONERROR);
}

static int set_root(const char *arg)
{
    char drive[4], fs[16];
    const char *p, *start;
    size_t len;
    UINT type;
    int quoted = *arg == '"';

    if (quoted)
        ++arg;
    len = strlen(arg);
    if (quoted) {
        if (!len || arg[len - 1] != '"')
            return 0;
        --len;
    }
    if (len < 3 || len + 1 + sizeof("blankeditor.iff") > sizeof(root) ||
        !((*arg >= 'A' && *arg <= 'Z') || (*arg >= 'a' && *arg <= 'z')) ||
        arg[1] != ':' || arg[2] != '\\')
        return 0;
    start = arg + 3;
    for (p = start; p < arg + len; p = CharNextA(p)) {
        if (*p == '\\') {
            if (p == start || p[-1] == '.' || p[-1] == ' ')
                return 0;
            start = p + 1;
        } else if ((unsigned char)*p < 32 || strchr("/:\"<>|?*", *p)) {
            return 0;
        }
    }
    if (p != arg + len || (p != start && (p[-1] == '.' || p[-1] == ' ')))
        return 0;
    memcpy(drive, arg, 3);
    drive[3] = '\0';
    type = GetDriveTypeA(drive);
    if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
        return 0;
    /* 9x doesn't support directory handles. Allow its local FAT only,
       where there are no NTFS reparse points or hard links. */
    win9x = (GetVersion() & 0x80000000UL) != 0;
    if (win9x && (!GetVolumeInformationA(drive, NULL, 0, NULL, NULL, NULL, fs, sizeof(fs)) ||
                  (strcmp(fs, "FAT") != 0 && strcmp(fs, "FAT32") != 0)))
        return 0;
    memcpy(root, arg, len);
    if (p != start)
        root[len++] = '\\';
    root[len] = '\0';
    return 1;
}

static const char *read_asset(const char *name, struct ilbm_image *image)
{
    char path[MAX_PATH], *p;
    HANDLE dirs[MAX_PATH / 2], file = INVALID_HANDLE_VALUE;
    unsigned count = 0;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD size, got, error;
    unsigned char *data = NULL;
    const char *problem;

    strcpy(path, root);
    if (!win9x) {
        /* Open each prefix without traversing a reparse point; keep it open
           without share-write/delete until the leaf has been read. */
        for (p = path + 3; *p; p = CharNextA(p)) {
            if (*p != '\\')
                continue;
            *p = '\0';
            dirs[count] = CreateFileA(path, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS |
                                      FILE_FLAG_OPEN_REPARSE_POINT, NULL);
            *p = '\\';
            problem = "Cannot open an asset-root directory.";
            if (dirs[count] == INVALID_HANDLE_VALUE)
                goto done;
            if (!GetFileInformationByHandle(dirs[count++], &info))
                goto done;
            problem = "Asset-root components must be directories, not reparse points.";
            if ((info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
                 FILE_ATTRIBUTE_REPARSE_POINT)) != FILE_ATTRIBUTE_DIRECTORY)
                goto done;
        }
    }
    strcat(path, name);
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       win9x ? FILE_ATTRIBUTE_NORMAL :
                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        problem = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
                  "Missing asset. Check the selected asset root." :
                  "Cannot open asset from the selected root.";
        goto done;
    }
    problem = "Asset must be a regular local file, not a directory or link.";
    if (GetFileType(file) != FILE_TYPE_DISK ||
        (!win9x && (!GetFileInformationByHandle(file, &info) || info.nNumberOfLinks != 1 ||
                    (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
                     FILE_ATTRIBUTE_REPARSE_POINT)))))
        goto done;
    size = GetFileSize(file, &got);
    problem = "Cannot determine a valid asset size (12 bytes to 4 MiB).";
    if (size < 12 || size > ILBM_MAX_FILE || got)
        goto done;
    problem = "Not enough memory to read asset.";
    data = malloc(size);
    if (!data)
        goto done;
    problem = "Cannot read complete asset.";
    if (ReadFile(file, data, size, &got, NULL) && got == size)
        problem = ilbm_decode(data, size, image);

done:
    free(data);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    while (count)
        CloseHandle(dirs[--count]);
    return problem;
}

static void open_picture(HWND window, unsigned which)
{
    struct ilbm_image image;
    size_t i, count;
    unsigned char *bgrx;
    char caption[64];
    const char *name = names[which], *problem = read_asset(name, &image);

    if (problem) {
        error_box(window, name, problem);
        return;
    }
    count = (size_t)image.width * image.height;
    bgrx = malloc(count * 4);
    if (!bgrx) {
        ilbm_free(&image);
        error_box(window, name, "Not enough memory for the display bitmap.");
        return;
    }
    for (i = 0; i < count; ++i) {
        bgrx[4 * i] = image.rgb[3 * i + 2];
        bgrx[4 * i + 1] = image.rgb[3 * i + 1];
        bgrx[4 * i + 2] = image.rgb[3 * i];
        bgrx[4 * i + 3] = 0;
    }
    ilbm_free(&image);
    free(pixels);
    pixels = bgrx;
    dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dib.bmiHeader.biWidth = image.width;
    dib.bmiHeader.biHeight = -(LONG)image.height;
    dib.bmiHeader.biPlanes = 1;
    dib.bmiHeader.biBitCount = 32;
    dib.bmiHeader.biCompression = BI_RGB;
    wsprintfA(caption, TITLE " - %s", name);
    SetWindowTextA(window, caption);
    InvalidateRect(window, NULL, TRUE);
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT paint;
    HDC dc;
    int width, height;

    switch (msg) {
    case WM_PAINT:
        dc = BeginPaint(window, &paint);
        if (pixels) {
            width = dib.bmiHeader.biWidth;
            height = -dib.bmiHeader.biHeight;
            StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, pixels,
                          &dib, DIB_RGB_COLORS, SRCCOPY);
        }
        EndPaint(window, &paint);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case OPEN_BACKGROUND: open_picture(window, 0); return 0;
        case OPEN_PICTURE: open_picture(window, 1); return 0;
        }
        break;
    case WM_DESTROY:
        free(pixels);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    WNDCLASSA wc = { 0 };
    RECT rect = { 0, 0, 320, 256 };
    HMENU menu, file_menu;
    HWND window;
    MSG msg;
    int status;

    (void)previous;
    if (!set_root(args)) {
        error_box(NULL, "Invalid asset root",
                  "Pass an absolute local drive folder, e.g. \"C:\\My Assets\".");
        return 2;
    }
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    menu = CreateMenu();
    file_menu = CreatePopupMenu();
    if (!RegisterClassA(&wc) || !menu || !file_menu ||
        !AppendMenuA(file_menu, MF_STRING, OPEN_BACKGROUND, "&Background (blankeditor.iff)") ||
        !AppendMenuA(file_menu, MF_STRING, OPEN_PICTURE, "&Picture (blueface.16)") ||
        !AppendMenuA(menu, MF_POPUP, (UINT_PTR)file_menu, "&File") ||
        !AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, TRUE)) {
        error_box(NULL, TITLE, "Cannot initialize viewer window.");
        return 1;
    }

    window = CreateWindowExA(0, CLASS_NAME, TITLE, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                             rect.bottom - rect.top, NULL, menu, instance, NULL);
    if (!window) {
        error_box(NULL, TITLE, "Cannot create viewer window.");
        return 1;
    }
    ShowWindow(window, show);
    open_picture(window, 0);
    while ((status = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return status < 0 ? 1 : (int)msg.wParam;
}

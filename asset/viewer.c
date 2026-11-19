/* Native milestone-2 viewer. Only these two leaf names can reach the loader. */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "ilbm.h"

#define TITLE "AmigaVid ILBM Viewer"
#define CLASS_NAME "AmigaVidAsset"
#define OPEN_BACKGROUND 100
#define OPEN_PICTURE 101

/* User-mode NtOpenFile ABI; resolved only on NT, not imported on 9x. */
#define NT_CASE_INSENSITIVE 0x40
#define NT_SYNCHRONOUS_IO_NONALERT 0x000020
#define NT_OPEN_REPARSE_POINT 0x200000
#define NT_NO_SUCH_FILE ((LONG)0xc000000fUL)
#define NT_OBJECT_NAME_NOT_FOUND ((LONG)0xc0000034UL)
#define NT_OBJECT_PATH_NOT_FOUND ((LONG)0xc000003aUL)

struct nt_name {
    USHORT length, capacity;
    WCHAR *text;
};

struct nt_object_attributes {
    ULONG length;
    HANDLE parent;
    struct nt_name *name;
    ULONG attributes;
    PVOID security_descriptor, security_qos;
};

struct nt_io_status {
    union {
        LONG status;
        PVOID pointer;
    } result;
    ULONG_PTR information;
};

typedef LONG (WINAPI *nt_open_file)(HANDLE *, ACCESS_MASK, struct nt_object_attributes *,
                                    struct nt_io_status *, ULONG, ULONG);
typedef DWORD (WINAPI *query_dos_device)(LPCWSTR, LPWSTR, DWORD);

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

static LONG nt_open(nt_open_file open_file, HANDLE parent, WCHAR *text, size_t len,
                    int directory, HANDLE *file)
{
    struct nt_name name;
    struct nt_object_attributes attrs = { 0 };
    struct nt_io_status io;

    name.length = name.capacity = (USHORT)(len * sizeof(*text));
    name.text = text;
    attrs.length = sizeof(attrs);
    attrs.parent = parent;
    attrs.name = &name;
    attrs.attributes = NT_CASE_INSENSITIVE;
    /* Check directory type on the returned handle instead of combining
       FILE_DIRECTORY_FILE with FILE_OPEN_REPARSE_POINT. */
    return open_file(file, directory ? FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES |
                     SYNCHRONIZE : FILE_GENERIC_READ,
                     &attrs, &io, FILE_SHARE_READ, NT_OPEN_REPARSE_POINT |
                     NT_SYNCHRONOUS_IO_NONALERT);
}

static int disk_device(const WCHAR *device)
{
    static const WCHAR *const prefixes[] = { L"\\Device\\HarddiskVolume", L"\\Device\\Floppy" };
    const WCHAR *p;
    unsigned i;
    size_t len;

    /* Only direct disk volume/floppy mappings, never aliases to a directory. */
    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        len = wcslen(prefixes[i]);
        if (_wcsnicmp(device, prefixes[i], len) == 0) {
            p = device + len;
            return *p && p[wcsspn(p, L"0123456789")] == L'\0';
        }
    }
    return 0;
}

static int ordinary_directory(HANDLE dir)
{
    BY_HANDLE_FILE_INFORMATION info;

    return GetFileType(dir) == FILE_TYPE_DISK && GetFileInformationByHandle(dir, &info) &&
           (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
            FILE_ATTRIBUTE_REPARSE_POINT)) == FILE_ATTRIBUTE_DIRECTORY;
}

static const char *open_nt_asset(const char *leaf, HANDLE *dirs, unsigned *count, HANDLE *file)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll"), kernel = GetModuleHandleA("kernel32.dll");
    nt_open_file open_file = ntdll ? (nt_open_file)GetProcAddress(ntdll, "NtOpenFile") : NULL;
    query_dos_device query = kernel ? (query_dos_device)GetProcAddress(kernel, "QueryDosDeviceW") : NULL;
    WCHAR drive[3], text[MAX_PATH];
    const char *start = root + 3, *p;
    size_t i, len;
    int n;
    LONG status;
    HANDLE handle;

    if (!open_file || !query)
        return "Cannot safely resolve asset paths on this system.";
    drive[0] = root[0];
    drive[1] = L':';
    drive[2] = L'\0';
    if (!query(drive, text, MAX_PATH) || !disk_device(text))
        return "Asset root must be on a directly mapped local disk.";
    len = wcslen(text);
    text[len++] = L'\\';  /* Open the filesystem root, not the volume itself. */
    if (nt_open(open_file, NULL, text, len, 1, &handle) != 0)
        return "Cannot open the asset drive root.";
    dirs[(*count)++] = handle;
    if (!ordinary_directory(handle))
        return "Asset drive root must be a local directory, not a reparse point.";

    /* Each lookup has exactly one name and the actual checked parent handle.
       Keep the chain open without share-write/delete until the read finishes. */
    for (p = start; *p; p = CharNextA(p)) {
        if (*p != '\\')
            continue;
        n = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, start, (int)(p - start),
                                text, MAX_PATH);
        if (!n || text[n - 1] == L'.' || text[n - 1] == L' ')
            return "Invalid asset-root component.";
        for (i = 0; i < (size_t)n; ++i) {
            if (text[i] < 32 || wcschr(L"\\/:\"<>|?*", text[i]))
                return "Invalid asset-root component.";
        }
        if (nt_open(open_file, dirs[*count - 1], text, n, 1, &handle) != 0)
            return "Cannot open an asset-root directory.";
        dirs[(*count)++] = handle;
        if (!ordinary_directory(handle))
            return "Asset-root components must be directories, not reparse points.";
        start = p + 1;
    }

    /* Both fixed ASCII leaf names use this same handle-relative open. */
    for (len = 0; leaf[len]; ++len)
        text[len] = leaf[len];
    status = nt_open(open_file, dirs[*count - 1], text, len, 0, &handle);
    if (status == NT_NO_SUCH_FILE || status == NT_OBJECT_NAME_NOT_FOUND ||
        status == NT_OBJECT_PATH_NOT_FOUND)
        return "Missing asset. Check the selected asset root.";
    if (status != 0)
        return "Cannot open asset from the selected root.";
    *file = handle;
    return NULL;
}

static const char *read_asset(const char *name, struct ilbm_image *image)
{
    HANDLE dirs[MAX_PATH / 2], file = INVALID_HANDLE_VALUE;
    unsigned count = 0;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD size, got;
    unsigned char *data = NULL;
    const char *problem;

    if (!win9x) {
        problem = open_nt_asset(name, dirs, &count, &file);
        if (problem)
            goto done;
    } else {
        char path[MAX_PATH];
        DWORD error;

        strcpy(path, root);
        strcat(path, name);
        file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            error = GetLastError();
            problem = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
                      "Missing asset. Check the selected asset root." :
                      "Cannot open asset from the selected root.";
            goto done;
        }
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

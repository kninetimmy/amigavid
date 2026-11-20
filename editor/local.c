#include "local.h"
#include <stdlib.h>
#include <string.h>
#include "vc10.h"

/* The input contract is the same on NT and FAT. On NT hold the checked
   ancestors without share-delete, refusing reparse points and aliases.
   On 9x directory handles are unavailable; require a local FAT volume. */
static char root[MAX_PATH];
static HANDLE held[MAX_PATH / 2];
static unsigned depth;
static int fat9x;

/* Same user-mode NtOpenFile ABI as the viewer; no NT-only static imports. */
struct nt_name { USHORT length, capacity; WCHAR *text; };
struct nt_attributes {
    ULONG length;
    HANDLE parent;
    struct nt_name *name;
    ULONG flags;
    PVOID security, qos;
};
struct nt_status { union { LONG status; PVOID pointer; } result; ULONG_PTR information; };
typedef LONG (WINAPI *nt_open_fn)(HANDLE *, ACCESS_MASK, struct nt_attributes *,
                                 struct nt_status *, ULONG, ULONG);
static nt_open_fn nt_open_file;

static void close_to(unsigned keep)
{
    while (depth > keep)
        CloseHandle(held[--depth]);
}

void local_close(void)
{
    close_to(0);
}

static HANDLE open_nt(HANDLE parent, WCHAR *text, unsigned length, int directory)
{
    struct nt_name name;
    struct nt_attributes attrs = { 0 };
    struct nt_status io;
    HANDLE file;

    name.length = name.capacity = (USHORT)(length * sizeof(WCHAR));
    name.text = text;
    attrs.length = sizeof(attrs);
    attrs.parent = parent;
    attrs.name = &name;
    attrs.flags = 0x40;     /* OBJ_CASE_INSENSITIVE */
    if (nt_open_file(&file, directory ? FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES |
                     SYNCHRONIZE : FILE_GENERIC_READ, &attrs, &io,
                     directory ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ,
                     0x200000 | 0x20) != 0)     /* OPEN_REPARSE_POINT | SYNCHRONOUS_IO_NONALERT */
        return INVALID_HANDLE_VALUE;
    return file;
}

static int ordinary(HANDLE handle, int directory)
{
    BY_HANDLE_FILE_INFORMATION info;
    return GetFileType(handle) == FILE_TYPE_DISK && GetFileInformationByHandle(handle, &info) &&
           (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
           == (directory ? FILE_ATTRIBUTE_DIRECTORY : 0) && (directory || info.nNumberOfLinks == 1);
}

static int hold_dir(HANDLE dir)
{
    if (dir == INVALID_HANDLE_VALUE)
        return 0;
    held[depth++] = dir;
    return ordinary(dir, 1);
}

/* Resolve directly mapped drive root, then hold each checked parent. */
static int hold_root(const char *drive)
{
    char target[1024];
    WCHAR wide[MAX_PATH];
    const char *p = target;
    unsigned n;
    typedef DWORD (WINAPI *query_fn)(LPCSTR, LPSTR, DWORD);
    query_fn query = (query_fn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "QueryDosDeviceA");

    if (!query || !query(drive, target, sizeof(target)))
        return 0;
    if (!strncmp(p, "\\Device\\HarddiskVolume", 22))
        p += 22;
    else if (!strncmp(p, "\\Device\\Floppy", 14))
        p += 14;
    else
        return 0;
    if (!*p || p[strspn(p, "0123456789")] || strlen(target) + 1 >= MAX_PATH)
        return 0;
    for (n = 0; target[n]; ++n)
        wide[n] = (unsigned char)target[n];
    wide[n++] = L'\\';
    return hold_dir(open_nt(NULL, wide, n, 1));
}

static int hold_path(const char **next, char separator)
{
    WCHAR wide[MAX_PATH];
    const char *p;
    unsigned n;

    while ((p = strchr(*next, separator)) != NULL) {
        for (n = 0; *next + n < p; ++n)
            wide[n] = (unsigned char)(*next)[n];
        if (!hold_dir(open_nt(held[depth - 1], wide, n, 1)))
            return 0;
        *next = p + 1;
    }
    return 1;
}

const char *local_init(const char *argument)
{
    char drive[4], fs[16], *p, *start;
    const char *next;
    HMODULE ntdll;
    size_t len;
    UINT type;
    DWORD attr;
    int quoted = *argument == '"';

    argument += quoted;
    len = strlen(argument);
    if (quoted && (!len || argument[--len] != '"'))
        return "Use one quoted workspace path.";
    if (len < 3 || len + VC_NAME + 8 >= MAX_PATH ||
        !((*argument >= 'A' && *argument <= 'Z') || (*argument >= 'a' && *argument <= 'z')) ||
        argument[1] != ':' || argument[2] != '\\')
        return "Use an absolute local drive folder, e.g. C:\\My Folder.";
    memcpy(root, argument, len);
    root[len] = 0;
    for (p = start = root + 3; *p; p = CharNextA(p)) {
        if (*p == '\\') {
            if (p == start || p[-1] == '.' || p[-1] == ' ')
                return "Workspace path has an invalid component.";
            start = p + 1;
        } else if ((unsigned char)*p < 32 || (unsigned char)*p > 126 || strchr("/:\"<>|?*", *p)) {
            return "Workspace path must use ordinary ASCII components.";
        }
    }
    if (p != start) {
        if (p[-1] == '.' || p[-1] == ' ')
            return "Workspace path has an invalid final component.";
        *p++ = '\\';
        *p = 0;
    }
    memcpy(drive, root, 3);
    drive[3] = 0;
    type = GetDriveTypeA(drive);
    if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
        return "Workspace must be on a local fixed or removable disk.";
    fat9x = (GetVersion() & 0x80000000UL) != 0;
    if (fat9x) {
        p = root + strlen(root) - 1;
        if (p > root + 2)
            *p = 0;
        attr = GetFileAttributesA(root);
        *p = '\\';
        if (!GetVolumeInformationA(drive, NULL, 0, NULL, NULL, NULL, fs, sizeof(fs)) ||
            (strcmp(fs, "FAT") && strcmp(fs, "FAT32")) || attr == INVALID_FILE_ATTRIBUTES ||
            !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return "Workspace must be an existing folder on local FAT.";
    } else {
        drive[2] = 0;
        next = root + 3;
        ntdll = GetModuleHandleA("ntdll.dll");
        nt_open_file = ntdll ? (nt_open_fn)GetProcAddress(ntdll, "NtOpenFile") : NULL;
        if (!nt_open_file || !hold_root(drive) || !hold_path(&next, '\\')) {
            local_close();
            return "Cannot pin an ordinary workspace folder (links are not supported).";
        }
    }
    return NULL;
}

static const char *open_read(const char *relative, HANDLE *file)
{
    char path[MAX_PATH], *p;
    WCHAR wide[VC_NAME];
    unsigned n;
    unsigned keep = depth;

    if (fat9x) {
        strcpy(path, root);
        strcat(path, relative);
        for (p = path + strlen(root); *p; ++p)
            if (*p == '/')
                *p = '\\';
        *file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    } else {
        if (!hold_path(&relative, '/')) {
            close_to(keep);
            return "Cannot open ordinary workspace subfolder.";
        }
        for (n = 0; relative[n]; ++n)
            wide[n] = (unsigned char)relative[n];
        *file = open_nt(held[depth - 1], wide, n, 0);
        close_to(keep);
    }
    if (*file == INVALID_HANDLE_VALUE)
        return "Cannot open local file.";
    if (fat9x ? GetFileType(*file) != FILE_TYPE_DISK : !ordinary(*file, 0)) {
        CloseHandle(*file);
        return "File must be an ordinary local file, not a directory or link.";
    }
    return NULL;
}

const char *local_read(const char *relative, DWORD limit, char **data, DWORD *size)
{
    HANDLE file;
    DWORD hi, got;
    const char *error = open_read(relative, &file);

    *data = NULL;
    if (error)
        return error;
    *size = GetFileSize(file, &hi);
    if (hi || !*size || *size > limit)
        error = "File is empty or larger than the supported bound.";
    else if (!(*data = malloc(*size)))
        error = "Not enough memory to read file.";
    else if (!ReadFile(file, *data, *size, &got, NULL) || got != *size)
        error = "Cannot read the complete file.";
    CloseHandle(file);
    if (error) {
        free(*data);
        *data = NULL;
    }
    return error;
}

const char *local_save(const char *leaf, const char *data, DWORD size, int replace)
{
    char path[MAX_PATH], backup[MAX_PATH], temp[MAX_PATH];
    HANDLE file;
    DWORD attr, wrote;
    int ok, had_project;
    unsigned i;
    const char *error;

    strcpy(path, root);
    strcat(path, leaf);
    strcpy(backup, path);
    strcat(backup, ".bak");
    attr = GetFileAttributesA(backup);
    if (attr != INVALID_FILE_ATTRIBUTES || GetLastError() != ERROR_FILE_NOT_FOUND)
        return "Backup already exists or cannot be checked. Inspect the .bak file first.";
    attr = GetFileAttributesA(path);
    had_project = attr != INVALID_FILE_ATTRIBUTES;
    if (!had_project && GetLastError() != ERROR_FILE_NOT_FOUND)
        return "Cannot check save destination.";
    if (had_project) {
        if (!replace)
            return "That filename exists. Open that project to replace it, or use a new name.";
        if (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY))
            return "Save destination is not an ordinary writable file.";
        error = open_read(leaf, &file);
        if (error)
            return error;
        CloseHandle(file);
    }
    for (i = 0; i < 100; ++i) {
        wsprintfA(temp, "%sVC%08lX-%u.tmp", root, GetCurrentProcessId(), i);
        file = CreateFileA(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS)
            break;
    }
    if (file == INVALID_HANDLE_VALUE)
        return "Cannot create a new staging file.";
    ok = WriteFile(file, data, size, &wrote, NULL) && wrote == size && FlushFileBuffers(file);
    if (!CloseHandle(file) || !ok) {
        DeleteFileA(temp);
        return "Cannot write and flush staged project; previous file was not moved.";
    }
    /* MoveFile on 9x never replaces. Keep the old project by name before
       publishing; never delete or overwrite a backup from an interrupted save. */
    if (had_project && !MoveFileA(path, backup)) {
        DeleteFileA(temp);
        return "Cannot back up previous project; it was not replaced.";
    }
    if (!MoveFileA(temp, path)) {
        DWORD code = GetLastError();
        static char detail[128];
        DeleteFileA(temp);
        if (had_project && !MoveFileA(backup, path))
            return "Save failed. Previous project is recoverable as .bak.";
        wsprintfA(detail, "Cannot publish staged project (Windows error %lu); previous file was preserved.", code);
        return detail;
    }
    if (had_project && !DeleteFileA(backup))
        return "Saved, but could not remove the previous project .bak. Inspect it before saving again.";
    return NULL;
}

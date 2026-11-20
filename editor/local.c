#include "local.h"
#include <stddef.h>
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
struct nt_rename {
    BOOLEAN replace;
    HANDLE parent;
    ULONG bytes;
    WCHAR name[VC_NAME + 4];
};
typedef LONG (WINAPI *nt_open_fn)(HANDLE *, ACCESS_MASK, struct nt_attributes *,
                                  struct nt_status *, ULONG, ULONG);
typedef LONG (WINAPI *nt_create_fn)(HANDLE *, ACCESS_MASK, struct nt_attributes *,
                                    struct nt_status *, LARGE_INTEGER *, ULONG,
                                    ULONG, ULONG, ULONG, PVOID, ULONG);
typedef LONG (WINAPI *nt_info_fn)(HANDLE, struct nt_status *, PVOID, ULONG, ULONG);
typedef LONG (WINAPI *nt_query_dir_fn)(HANDLE, HANDLE, PVOID, PVOID, struct nt_status *,
                                       PVOID, ULONG, ULONG, BOOLEAN, struct nt_name *, BOOLEAN);
typedef LONG (WINAPI *nt_query_security_fn)(HANDLE, SECURITY_INFORMATION, PSECURITY_DESCRIPTOR,
                                           ULONG, ULONG *);
static nt_open_fn nt_open_file;
static nt_create_fn nt_create_file;
static nt_info_fn nt_set_file;
static nt_info_fn nt_query_file;
static nt_query_dir_fn nt_query_dir;
static nt_query_security_fn nt_query_security;

#define NT_OPEN_CHECKED (0x200000 | 0x20) /* OPEN_REPARSE_POINT | SYNCHRONOUS_IO_NONALERT */
#define NT_NAME_MISSING ((LONG)0xc0000034UL)
#define NT_NAME_COLLISION ((LONG)0xc0000035UL)
#define NT_BUFFER_TOO_SMALL ((LONG)0xc0000023UL)
#define NT_RENAME 10                     /* FileRenameInformation */
#define NT_DISPOSITION 13                /* FileDispositionInformation */
#define NT_STREAM 22                     /* FileStreamInformation */
#define NT_ID_BOTH_DIR 37                /* FileIdBothDirectoryInformation */

static void close_to(unsigned keep)
{
    while (depth > keep)
        CloseHandle(held[--depth]);
}

void local_close(void)
{
    close_to(0);
}

static void nt_name_at(struct nt_attributes *attrs, struct nt_name *name,
                       HANDLE parent, WCHAR *text, unsigned length)
{
    name->length = name->capacity = (USHORT)(length * sizeof(WCHAR));
    name->text = text;
    attrs->length = sizeof(*attrs);
    attrs->parent = parent;
    attrs->name = name;
    attrs->flags = 0x40;     /* OBJ_CASE_INSENSITIVE */
}

static HANDLE open_nt(HANDLE parent, WCHAR *text, unsigned length, int directory)
{
    struct nt_name name;
    struct nt_attributes attrs = { 0 };
    struct nt_status io;
    HANDLE file;

    nt_name_at(&attrs, &name, parent, text, length);
    if (nt_open_file(&file, directory ? FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES |
                      SYNCHRONIZE : FILE_GENERIC_READ, &attrs, &io,
                      directory ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ,
                      NT_OPEN_CHECKED) != 0)
        return INVALID_HANDLE_VALUE;
    return file;
}

static int ordinary(HANDLE handle, int directory, DWORD extra_forbidden)
{
    BY_HANDLE_FILE_INFORMATION info;
    return GetFileType(handle) == FILE_TYPE_DISK && GetFileInformationByHandle(handle, &info) &&
           (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT |
                                     extra_forbidden))
           == (directory ? FILE_ATTRIBUTE_DIRECTORY : 0) && (directory || info.nNumberOfLinks == 1);
}

static int hold_dir(HANDLE dir)
{
    if (dir == INVALID_HANDLE_VALUE)
        return 0;
    held[depth++] = dir;
    return ordinary(dir, 1, 0);
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
        nt_create_file = ntdll ? (nt_create_fn)GetProcAddress(ntdll, "NtCreateFile") : NULL;
        nt_set_file = ntdll ? (nt_info_fn)GetProcAddress(ntdll, "NtSetInformationFile") : NULL;
        nt_query_file = ntdll ? (nt_info_fn)GetProcAddress(ntdll, "NtQueryInformationFile") : NULL;
        nt_query_dir = ntdll ? (nt_query_dir_fn)GetProcAddress(ntdll, "NtQueryDirectoryFile") : NULL;
        nt_query_security = ntdll ? (nt_query_security_fn)GetProcAddress(ntdll, "NtQuerySecurityObject") : NULL;
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
    if (fat9x ? GetFileType(*file) != FILE_TYPE_DISK : !ordinary(*file, 0, 0)) {
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

/* NT mutations use the held workspace, not the launch-time drive spelling.
   Open existing entries without following links and keep the exact file
   handles through rename, rollback and delete. No replace-by-name flags. */
static LONG save_open_nt(HANDLE *file, const char *leaf, ACCESS_MASK access, ULONG share, int create)
{
    WCHAR wide[VC_NAME + 4];
    struct nt_attributes attrs = { 0 };
    struct nt_name name;
    struct nt_status io;
    unsigned n;

    for (n = 0; leaf[n]; ++n)
        wide[n] = (unsigned char)leaf[n];
    nt_name_at(&attrs, &name, held[depth - 1], wide, n);
    if (create)
        return nt_create_file(file, access, &attrs, &io, NULL, FILE_ATTRIBUTE_NORMAL, share,
                              2, NT_OPEN_CHECKED | 0x40, NULL, 0); /* FILE_CREATE | NON_DIRECTORY_FILE */
    return nt_open_file(file, access, &attrs, &io, share, NT_OPEN_CHECKED);
}

static LONG rename_nt(HANDLE file, const char *leaf)
{
    struct nt_rename rename = { 0 };
    struct nt_status io;
    unsigned n;

    rename.parent = held[depth - 1];
    for (n = 0; leaf[n]; ++n)
        rename.name[n] = (unsigned char)leaf[n];
    rename.bytes = n * sizeof(WCHAR);
    return nt_set_file(file, &io, &rename, offsetof(struct nt_rename, name) +
                       rename.bytes + sizeof(WCHAR), NT_RENAME);
}

static LONG delete_nt(HANDLE file)
{
    struct nt_status io;
    BOOLEAN yes = TRUE;

    return nt_set_file(file, &io, &yes, sizeof(yes), NT_DISPOSITION);
}

static const char backup_exists[] = "Backup already exists or cannot be checked. Inspect the .bak file first.";
static const char backup_retained[] =
    "Saved, but could not remove the previous project .bak. Inspect it before saving again.";
static const char recovery[] = "Save failed. Previous project is recoverable as .bak.";
static const char exists[] = "That filename exists. Open that project to replace it, or use a new name.";
static const char bad_destination[] = "Save destination is not an ordinary writable file.";
static const char bad_name[] = "Cannot verify the project filename. Open it by its full name to replace it.";
static const char bad_security[] = "Cannot preserve the previous project's access permissions. Use a new name.";
static const char bad_streams[] = "Project has extra data streams or they could not be checked. Use a new name.";

static unsigned fold_ascii(unsigned ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

/* Reopen the held directory itself for a fresh, exact search expression.
   Match the long name of the already-open file, not its optional 8.3 name. */
static int named_nt(HANDLE prior, const char *leaf)
{
    struct nt_dir_entry {
        ULONG next, index;
        LARGE_INTEGER times_and_sizes[6];
        ULONG attributes, bytes, ea_size;
        CCHAR short_bytes;
        WCHAR short_name[12];
        LARGE_INTEGER id;
        WCHAR text[MAX_PATH];
    } entry;
    BY_HANDLE_FILE_INFORMATION info;
    struct nt_attributes attrs = { 0 };
    struct nt_name name;
    struct nt_status io;
    WCHAR wide[VC_NAME];
    HANDLE dir;
    LONG status;
    unsigned n;

    nt_name_at(&attrs, &name, held[depth - 1], wide, 0);
    if (!nt_query_dir || !GetFileInformationByHandle(prior, &info) ||
        nt_open_file(&dir, FILE_LIST_DIRECTORY | SYNCHRONIZE,
                                      &attrs, &io, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NT_OPEN_CHECKED | 1) != 0) /* DIRECTORY_FILE */
        return 0;
    for (n = 0; leaf[n]; ++n)
        wide[n] = (unsigned char)leaf[n];
    name.length = name.capacity = (USHORT)(n * sizeof(WCHAR));
    status = nt_query_dir(dir, NULL, NULL, NULL, &io, &entry, sizeof(entry),
                          NT_ID_BOTH_DIR, TRUE, &name, TRUE);
    CloseHandle(dir);
    if (status != 0 || io.information < offsetof(struct nt_dir_entry, text) + name.length ||
        entry.id.LowPart != info.nFileIndexLow || (DWORD)entry.id.HighPart != info.nFileIndexHigh ||
        entry.bytes != name.length)
        return 0;
    for (n = 0; leaf[n]; ++n)
        if (fold_ascii(entry.text[n]) != fold_ascii((unsigned char)leaf[n]))
            return 0;
    return 1;
}

static int named_fat(const char *path, const char *leaf)
{
    WIN32_FIND_DATAA entry;
    HANDLE search = FindFirstFileA(path, &entry);
    unsigned n;

    if (search == INVALID_HANDLE_VALUE)
        return 0;
    FindClose(search);
    for (n = 0; leaf[n]; ++n)
        if (fold_ascii((unsigned char)entry.cFileName[n]) != fold_ascii((unsigned char)leaf[n]))
            return 0;
    return entry.cFileName[n] == 0;
}

static int only_default_stream(HANDLE prior)
{
    static const WCHAR default_name[] = L"::$DATA";
    struct nt_stream_entry {
        ULONG next, bytes;
        LARGE_INTEGER size, allocated;
        WCHAR name[sizeof(default_name) / sizeof(WCHAR)];
    } entry;
    /* The native result buffer must be aligned to 8 bytes, also on x86. */
    char storage[sizeof(entry) + 7];
    void *buffer = (void *)(((ULONG_PTR)storage + 7) & ~(ULONG_PTR)7);
    struct nt_status io = { 0 };
    const size_t header = offsetof(struct nt_stream_entry, name);

    /* Space for one default stream only. Overflow is not proof of absence. */
    if (!nt_query_file || nt_query_file(prior, &io, buffer, sizeof(entry), NT_STREAM) != 0 ||
        io.information < header || io.information > sizeof(entry))
        return 0;
    memcpy(&entry, buffer, io.information);
    return !entry.next && (!entry.bytes || (entry.bytes == sizeof(default_name) - sizeof(WCHAR) &&
           io.information - header >= entry.bytes && !memcmp(entry.name, default_name, entry.bytes)));
}

static void *read_security(HANDLE file, ULONG *size)
{
    SECURITY_INFORMATION fields = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                                  DACL_SECURITY_INFORMATION;
    DWORD version = GetVersion();
    void *result;
    ULONG actual;

    /* Vista and later also enforce an integrity label (XP does not). */
    if (LOBYTE(version) >= 6)
        fields |= 0x10; /* LABEL_SECURITY_INFORMATION */
    if (LOBYTE(version) > 6 || (LOBYTE(version) == 6 && HIBYTE(version) >= 2))
        fields |= 0x20 | 0x40; /* ATTRIBUTE_SECURITY_INFORMATION | SCOPE_SECURITY_INFORMATION */
    if (!nt_query_security || nt_query_security(file, fields, NULL, 0, size) != NT_BUFFER_TOO_SMALL ||
        !*size || !(result = calloc(1, *size)))
        return NULL;
    if (nt_query_security(file, fields, result, *size, &actual) != 0 || actual != *size) {
        free(result);
        return NULL;
    }
    return result;
}

static int same_security(HANDLE prior, HANDLE staged)
{
    ULONG old_size, new_size;
    void *old = read_security(prior, &old_size), *fresh = read_security(staged, &new_size);
    int same = old && fresh && old_size == new_size && !memcmp(old, fresh, old_size);

    free(old);
    free(fresh);
    return same;
}

static int write_staged(HANDLE file, const char *data, DWORD size)
{
    DWORD wrote;
    return WriteFile(file, data, size, &wrote, NULL) && wrote == size && FlushFileBuffers(file);
}

static const char *save_nt(const char *leaf, const char *data, DWORD size, int replace, int *published)
{
    char backup[VC_NAME + 4], temp[VC_NAME];
    HANDLE prior = INVALID_HANDLE_VALUE, staged, probe;
    LONG status;
    unsigned i;
    const char *error;

    if (!depth || !nt_create_file || !nt_set_file)
        return "Handle-relative workspace writes are unavailable.";
    wsprintfA(backup, "%s.bak", leaf);
    status = save_open_nt(&probe, backup, FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (status == 0)
        CloseHandle(probe);
    if (status != NT_NAME_MISSING)
        return backup_exists;

    status = save_open_nt(&prior, leaf, replace ? FILE_GENERIC_READ | DELETE :
                          FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ, 0);
    if (status == NT_NAME_MISSING) {
        prior = INVALID_HANDLE_VALUE;
    } else if (status != 0) {
        return replace ? "Cannot back up previous project; it was not replaced." :
                         "Cannot check save destination.";
    } else if (!replace || !ordinary(prior, 0, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ENCRYPTED)) {
        CloseHandle(prior);
        return replace ? bad_destination : exists;
    } else if (!named_nt(prior, leaf)) {
        CloseHandle(prior);
        return bad_name;
    } else if (!only_default_stream(prior)) {
        CloseHandle(prior);
        return bad_streams;
    }

    for (i = 0; i < 100; ++i) {
        wsprintfA(temp, "VC%08lX-%u.tmp", GetCurrentProcessId(), i);
        status = save_open_nt(&staged, temp, FILE_GENERIC_WRITE | DELETE, 0, 1);
        if (status != NT_NAME_COLLISION)
            break;
    }
    if (status != 0) {
        error = "Cannot create a new staging file.";
    } else {
        /* Do not put any project bytes into a differently protected file.
           Be conservative: we do not try to translate or copy custom ACLs. */
        if (prior != INVALID_HANDLE_VALUE && !same_security(prior, staged)) {
            error = bad_security;
        } else if (!write_staged(staged, data, size)) {
            error = "Cannot write and flush staged project; previous file was not moved.";
        } else if (prior != INVALID_HANDLE_VALUE && rename_nt(prior, backup) != 0) {
            error = "Cannot back up previous project; it was not replaced.";
        } else if (rename_nt(staged, leaf) != 0) {
            if (prior != INVALID_HANDLE_VALUE)
                error = rename_nt(prior, leaf) == 0 ?
                    "Cannot publish staged project; previous file was restored." : recovery;
            else
                error = "Cannot publish staged project.";
        } else {
            *published = 1;
            error = prior != INVALID_HANDLE_VALUE && delete_nt(prior) != 0 ? backup_retained : NULL;
        }
        if (!*published)
            delete_nt(staged);
        CloseHandle(staged);
    }
    if (prior != INVALID_HANDLE_VALUE)
        CloseHandle(prior);
    return error;
}

static const char *save_fat(const char *leaf, const char *data, DWORD size, int replace, int *published)
{
    char path[MAX_PATH], backup[MAX_PATH], temp[MAX_PATH];
    HANDLE file;
    DWORD attr;
    int ok, had_project;
    unsigned i;
    const char *error;

    strcpy(path, root);
    strcat(path, leaf);
    strcpy(backup, path);
    strcat(backup, ".bak");
    attr = GetFileAttributesA(backup);
    if (attr != INVALID_FILE_ATTRIBUTES || GetLastError() != ERROR_FILE_NOT_FOUND)
        return backup_exists;
    attr = GetFileAttributesA(path);
    had_project = attr != INVALID_FILE_ATTRIBUTES;
    if (!had_project && GetLastError() != ERROR_FILE_NOT_FOUND)
        return "Cannot check save destination.";
    if (had_project) {
        if (!replace)
            return exists;
        if (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY))
            return bad_destination;
        if (!named_fat(path, leaf))
            return bad_name;
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
    ok = write_staged(file, data, size);
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
            return recovery;
        wsprintfA(detail, "Cannot publish staged project (Windows error %lu).", code);
        return detail;
    }
    *published = 1;
    if (had_project && !DeleteFileA(backup))
        return backup_retained;
    return NULL;
}

const char *local_save(const char *leaf, const char *data, DWORD size, int replace, int *published)
{
    *published = 0;
    if (vc_picture_path(leaf) != leaf || strchr(leaf, '/'))
        return "Save destination must be an ordinary workspace filename.";
    return fat9x ? save_fat(leaf, data, size, replace, published) :
                   save_nt(leaf, data, size, replace, published);
}

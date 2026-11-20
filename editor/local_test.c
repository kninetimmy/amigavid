/* Host-only driver: exercise real NT I/O in two scratch directories. */
#include "local.c"
#include <stdio.h>

static nt_set_fn real_set_file;
static unsigned fail_renames;
static int fail_delete;
static unsigned rename_calls, empty_cleanups;

static LONG WINAPI controlled_set(HANDLE file, struct nt_status *io, PVOID data,
                                  ULONG size, ULONG operation)
{
    int fail = operation == NT_DISPOSITION && fail_delete;

    if (operation == NT_RENAME) {
        ++rename_calls;
        fail = fail_renames & 1;
        fail_renames >>= 1;
    } else if (operation == NT_DISPOSITION) {
        DWORD high;
        if (GetFileSize(file, &high) == 0 && !high)
            ++empty_cleanups;
    }
    if (fail)
        return (LONG)0xc0000022UL; /* STATUS_ACCESS_DENIED */
    return real_set_file(file, io, data, size, operation);
}

static void need(int condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "local test: %s\n", description);
        exit(1);
    }
}

static int matches(const char *name, const char *expected)
{
    DWORD size;
    char *actual;
    int ok = !local_read(name, 32, &actual, &size) &&
             size == strlen(expected) && !memcmp(actual, expected, size);
    free(actual);
    return ok;
}

int main(int argc, char **argv)
{
    int published;
    const char *notice;
    const char *leaf = "test.vid", *backup = "test.vid.bak";
    WIN32_FIND_DATAA entry;
    HANDLE search;
    char long_path[MAX_PATH];
    unsigned before;
    nt_query_dir_fn query_dir = NULL;
    nt_query_security_fn query_security = NULL;

    need(argc == 3 && strlen(argv[2]) + 2 <= sizeof(root), "two scratch paths required");
    notice = local_init(argv[1]);
    need(!notice, notice);
    need(!fat9x && nt_set_file, "native NT handle-based API required");

    /* Model the drive spelling resolving to a different workspace after
       initial pinning. The Python driver checks both scratch directories. */
    strcpy(root, argv[2]);
    strcat(root, "\\");
    notice = local_save(leaf, "old", 3, 0, &published);
    need(published && !notice && matches(leaf, "old"), "first save uses pinned directory");

    real_set_file = nt_set_file;
    nt_set_file = controlled_set;
    notice = local_save("private.vid", "new", 3, 1, &published);
    need(!published && notice == bad_security, notice ? notice : "different DACL must refuse");
    need(!rename_calls && empty_cleanups == 1 && matches("private.vid", "old"),
         "different DACL refuses with empty staging and no rename");

    leaf = "Long Project File.vid";
    wsprintfA(long_path, "%s\\%s", argv[1], leaf);
    search = FindFirstFileA(long_path, &entry);
    need(search != INVALID_HANDLE_VALUE, "find long-name fixture");
    FindClose(search);
    if (entry.cAlternateFileName[0]) {
        before = rename_calls;
        need(matches(entry.cAlternateFileName, "old"), "real 8.3 alias identifies the fixture");
        notice = local_save(entry.cAlternateFileName, "new", 3, 1, &published);
        need(!published && notice == bad_name && rename_calls == before && matches(leaf, "old"),
             "true alias refuses before rename");
        puts("PASS actual NT 8.3 alias refuses replacement without renaming the canonical project");
    } else {
        puts("SKIP host NT 8.3 alias: scratch volume did not generate a short name");
    }
    notice = local_save(leaf, "new", 3, 1, &published);
    need(published && !notice && matches(leaf, "new"), "long-name replacement succeeds");
    leaf = "LIT~1.VID";
    notice = local_save(leaf, "old", 3, 0, &published);
    need(published && !notice, "prepare literal tilde fixture");
    notice = local_save(leaf, "new", 3, 1, &published);
    need(published && !notice && matches(leaf, "new"), "literal tilde replacement succeeds");
    puts("PASS different NT DACL refuses before writing; ordinary long and literal tilde replacements succeed");

    before = rename_calls;
    query_dir = nt_query_dir;
    nt_query_dir = NULL;
    notice = local_save(leaf, "later", 5, 1, &published);
    need(!published && notice == bad_name && rename_calls == before, "missing canonical-name API must refuse");
    nt_query_dir = query_dir;
    query_security = nt_query_security;
    nt_query_security = NULL;
    notice = local_save(leaf, "later", 5, 1, &published);
    need(!published && notice == bad_security && rename_calls == before && empty_cleanups == 2 &&
         matches(leaf, "new"), "missing protection API must refuse before writing");
    nt_query_security = query_security;
    puts("PASS missing NT metadata APIs refuse replacement without writing or renaming");

    leaf = "test.vid";
    fail_renames = 2; /* Fail publish, allow rollback. */
    notice = local_save(leaf, "new", 3, 1, &published);
    need(!published && notice && strstr(notice, "previous file was restored") &&
         matches(leaf, "old"), "successful rollback after backup");

    fail_renames = 6; /* Fail both publish and rollback. */
    notice = local_save(leaf, "new", 3, 1, &published);
    need(!published && notice == recovery && matches(backup, "old") &&
         !matches(leaf, "old"), "failed rollback preserves the backup");
    notice = local_save(leaf, "new", 3, 1, &published);
    need(!published && notice == backup_exists, "retry does not consume backup");

    fail_delete = 1;
    leaf = "warning.vid";
    notice = local_save(leaf, "old", 3, 0, &published);
    need(published && !notice, "prepare cleanup-warning case");
    notice = local_save(leaf, "new", 3, 1, &published);
    need(published && notice == backup_retained && matches(leaf, "new") &&
         matches("warning.vid.bak", "old"), "backup cleanup failure is a published save");

    nt_set_file = NULL;
    notice = local_save("refuse.vid", "new", 3, 0, &published);
    need(!published && notice, "missing native API must refuse save");
    local_close();
    return 0;
}

/* Host-only driver: exercise the real NT I/O with a synthetic path change
   and controlled failures, never remap a drive or touch a live project. */
#include "local.c"
#include <stdio.h>

static nt_set_fn real_set_file;
static unsigned fail_renames;
static int fail_delete;

static LONG WINAPI controlled_set(HANDLE file, struct nt_status *io, PVOID data,
                                  ULONG size, ULONG operation)
{
    int fail = operation == NT_DISPOSITION && fail_delete;

    if (operation == NT_RENAME) {
        fail = fail_renames & 1;
        fail_renames >>= 1;
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

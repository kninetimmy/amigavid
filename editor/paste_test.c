/* Optional, disposable XP guest ONLY; never run against the host clipboard.
   Build with the pinned Watcom Win32 GUI profile used by tools/editor.py.
   In a fresh guest workspace containing PIC.IFF and the chosen editor:
     PASTETST.EXE --disposable-xp-guest OLD.EXE OLD.LOG
   No QEMU/clipboard dependency is added to the ordinary host checks. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *logfile;
static DWORD editor_pid;
static HWND found;
static unsigned failures;

static const char baseline[] =
    "VC10\xa9 Almathera Ltd 1994\nAUTHOR\n1\n1\nWDQjdt;QJD/JGG\nZIP\n"
    "0\n0\n0\n1\n0\n1\n6\n0\n0\n0\n0\n0\n0\nNOTEXT\n";

static BOOL CALLBACK visit(HWND hwnd, LPARAM popup)
{
    DWORD pid;
    char name[80];
    GetWindowThreadProcessId(hwnd, &pid);
    GetClassNameA(hwnd, name, sizeof(name));
    if (pid == editor_pid && IsWindowVisible(hwnd) &&
        !strcmp(name, popup ? "#32770" : "AmigaVidEditor"))
        found = hwnd;
    return TRUE;
}

static HWND find(int popup)
{
    found = NULL;
    EnumWindows(visit, popup);
    return found;
}

static void fatal(const char *why)
{
    fprintf(logfile, "FATAL %s (Win32 %lu)\n", why, GetLastError());
    fflush(logfile);
    ExitProcess(2);
}

static LRESULT control_message(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    DWORD result;
    if (!hwnd || !SendMessageTimeoutA(hwnd, message, wp, lp,
                                     SMTO_ABORTIFHUNG, 10000, &result))
        fatal("control message failed");
    return (LRESULT)result;
}

static void edit(HWND hwnd, unsigned id, const char *text)
{
    control_message(GetDlgItem(hwnd, id), WM_SETTEXT, 0, (LPARAM)text);
}

static void click(HWND hwnd, unsigned id)
{
    control_message(GetDlgItem(hwnd, id), BM_CLICK, 0, 0);
}

static HWND wait_window(int popup)
{
    DWORD start = GetTickCount();
    HWND hwnd;
    do {
        hwnd = find(popup);
        if (hwnd)
            return hwnd;
        Sleep(20);
    } while (GetTickCount() - start < 20000);
    fatal("editor window/dialog timeout");
    return NULL;
}

static void paste(HWND owner, HWND field, const char *text, int start, int end)
{
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
    void *bytes;
    if (!data || !(bytes = GlobalLock(data)))
        fatal("clipboard allocation");
    strcpy(bytes, text);
    GlobalUnlock(data);
    if (!OpenClipboard(owner) || !EmptyClipboard() ||
        !SetClipboardData(CF_TEXT, data) || !CloseClipboard())
        fatal("guest clipboard setup");
    control_message(field, EM_SETSEL, start, end);
    control_message(field, WM_PASTE, 0, 0);
}

static int read_file(const char *name, char *bytes, unsigned size)
{
    unsigned n;
    FILE *file = fopen(name, "rb");
    if (!file)
        fatal("saved project missing");
    n = fread(bytes, 1, size, file);
    if (ferror(file) || n == size)
        fatal("saved project read/size");
    fclose(file);
    return (int)n;
}

static int fixture_names(const char *exe, const char *report)
{
    /* Fixed leaves cannot escape the guest workspace or name DOS devices. */
    return (!strcmp(exe, "VCEDIT.EXE") && !strcmp(report, "PASTE.LOG")) ||
           (!strcmp(exe, "OLD.EXE") && !strcmp(report, "OLD.LOG")) ||
           (!strcmp(exe, "NEW.EXE") && !strcmp(report, "NEW.LOG"));
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    static const struct {
        const char *name, *initial, *paste;
        int start, end, valid;
    } cases[] = {
        { "replace-all-1999", "25", "1999", 0, -1, 0 },
        { "replace-last-1999", "125", "999", 1, 3, 0 },
        { "insert-1999", "199", "9", 3, 3, 0 },
        { "replace-middle-1999", "1259", "99", 1, 3, 0 },
        { "long-paste", "25",
          "22222222222222222222222222222222222222222222222222"
          "22222222222222222222222222222222222222222222222222", 0, -1, 0 },
        { "leading-space", "25", " 2", 0, -1, 0 },
        { "decimal", "25", "2.5", 0, -1, 0 },
        { "zero", "25", "0", 0, -1, 0 },
        { "out-of-range", "25", "256", 0, -1, 0 },
        { "valid-1", "25", "1", 0, -1, 1 },
        { "valid-25", "1", "25", 0, -1, 25 },
        { "valid-255", "25", "255", 0, -1, 255 },
        { "replace-last-valid-255", "25", "55", 1, 2, 255 },
        { "replace-all-crlf", "7", "25\r\n256", 0, -1, 0 },
        { "replace-last-crlf", "125", "2\r\n256", 1, 3, 0 },
        { "replace-all-lf", "7", "25\n256", 0, -1, 0 },
        { "replace-last-lf", "125", "2\n256", 1, 3, 0 },
        { "replace-all-cr", "7", "25\r256", 0, -1, 0 },
        { "replace-last-cr", "125", "2\r256", 1, 3, 0 },
        { "trailing-crlf", "7", "25\r\n", 0, -1, 0 }
    };
    static const unsigned buttons[] = { 104, 112, 113, 114, 115, 116, 117, 118 };
    OSVERSIONINFOA version = { sizeof(OSVERSIONINFOA) };
    char flag[40], exe[13], report[13], extra[2], root[MAX_PATH], path[MAX_PATH];
    char command[MAX_PATH * 2 + 8], actual[256], observed[1024], expected[1024];
    char output[13], popup_text[160], full_text[256];
    HWND hwnd, popup, field, owner;
    STARTUPINFOA startup = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION process;
    FILE *file;
    unsigned effect, index, n, wanted;
    const char *ch;
    int rejected, same, retained, whole;
    DWORD code;
    (void)instance; (void)previous; (void)show;

    /* Two independent explicit guards precede every clipboard and file write. */
    if (!GetVersionExA(&version) || version.dwPlatformId != VER_PLATFORM_WIN32_NT ||
        version.dwMajorVersion != 5 || version.dwMinorVersion != 1 ||
        sscanf(args, "%39s %12s %12s %1s", flag, exe, report, extra) != 3 ||
        strcmp(flag, "--disposable-xp-guest") || !fixture_names(exe, report))
        return 2;
    logfile = fopen(report, "wb");
    if (!logfile)
        return 2;
    fprintf(logfile, "Disposable XP %lu.%lu build %lu %s; editor=%s\n",
            version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber,
            version.szCSDVersion, exe);
    code = GetCurrentDirectoryA(sizeof(root), root);
    if (!code || code >= sizeof(root) ||
        strlen(root) + strlen(exe) + 2 >= sizeof(path))
        fatal("workspace path");
    wsprintfA(path, "%s\\%s", root, exe);
    file = fopen("BASE.VID", "wb");
    if (!file || fwrite(baseline, 1, sizeof(baseline) - 1, file) != sizeof(baseline) - 1)
        fatal("baseline write");
    fclose(file);
    owner = CreateWindowExA(0, "STATIC", "Disposable paste test", 0,
                            0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (!owner)
        fatal("clipboard owner");
    for (effect = 2; effect <= 9; ++effect) {
        wsprintfA(command, "\"%s\" \"%s\"", path, root);
        if (!CreateProcessA(path, command, NULL, NULL, FALSE, 0, NULL, root, &startup, &process))
            fatal("editor launch");
        editor_pid = process.dwProcessId;
        hwnd = wait_window(0);
        for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
            edit(hwnd, 109, "BASE.VID");
            click(hwnd, 101);
            edit(hwnd, 110, cases[index].initial);
            field = GetDlgItem(hwnd, 110);
            paste(owner, field, cases[index].paste, cases[index].start, cases[index].end);
            control_message(field, WM_GETTEXT, sizeof(actual), (LPARAM)actual);
            memcpy(full_text, cases[index].initial, cases[index].start);
            full_text[cases[index].start] = 0;
            strcat(full_text, cases[index].paste);
            strcat(full_text, cases[index].initial +
                   (cases[index].end < 0 ? strlen(cases[index].initial) : (unsigned)cases[index].end));
            whole = !strcmp(actual, full_text);
            if (!PostMessageA(GetDlgItem(hwnd, buttons[effect - 2]), BM_CLICK, 0, 0))
                fatal("post Add command");
            /* A modal error or changed status completes the normal command. */
            for (n = 0; n < 1000; ++n) {
                popup = find(1);
                if (popup)
                    break;
                /* Status has ID 0 alongside labels: enumerate via sibling order. */
                field = GetWindow(hwnd, GW_CHILD);
                while (field) {
                    GetWindowTextA(field, popup_text, sizeof(popup_text));
                    if (!strcmp(popup_text, "Cue added. Drag its timeline marker to change the start tick."))
                        break;
                    field = GetWindow(field, GW_HWNDNEXT);
                }
                if (field)
                    break;
                Sleep(20);
            }
            if (n == 1000)
                fatal("add cue timeout");
            rejected = popup != NULL;
            if (popup) {
                GetDlgItemTextA(popup, 65535, popup_text, sizeof(popup_text));
                if (!strstr(popup_text, "effect duration of 1 to 255"))
                    fatal("wrong rejection dialog");
                if (!PostMessageA(popup, WM_CLOSE, 0, 0))
                    fatal("dismiss rejection");
                for (n = 0; n < 1000 && IsWindowVisible(popup); ++n)
                    Sleep(20);
                if (IsWindowVisible(popup))
                    fatal("dialog dismissal timeout");
            }
            wsprintfA(output, "C%u%02u.VID", effect, index);
            edit(hwnd, 109, output);
            click(hwnd, 102);
            n = (unsigned)read_file(output, observed, sizeof(observed));
            strcpy(expected, baseline);
            if (cases[index].valid) {
                char *count = strstr(expected, "\n0\n0\n0\n1\n");
                if (!count)
                    fatal("baseline cue count");
                count[7] = '2';
                wsprintfA(expected + strlen(expected), "25\n%u\n6\n%u\n0\n0\n0\n0\n0\nNOTEXT\n",
                          effect, cases[index].valid);
            }
            wanted = (unsigned)strlen(expected);
            same = n == wanted && !memcmp(observed, expected, wanted);
            retained = !cases[index].valid ? rejected : !rejected;
            fprintf(logfile, "%s ID=%u %s field=[",
                    same && retained && whole ? "PASS" : "FAIL", effect, cases[index].name);
            for (ch = actual; *ch; ++ch)
                if (*ch == '\r' || *ch == '\n')
                    fputs(*ch == '\r' ? "\\r" : "\\n", logfile);
                else
                    fputc(*ch, logfile);
            fprintf(logfile, "] whole=%d rejected=%d bytes=%u expected=%u exact=%d\n",
                    whole, rejected, n, wanted, same);
            fflush(logfile);
            if (!same || !retained || !whole)
                ++failures;
        }
        control_message(hwnd, WM_CLOSE, 0, 0);
        if (WaitForSingleObject(process.hProcess, 10000) != WAIT_OBJECT_0 ||
            !GetExitCodeProcess(process.hProcess, &code) || code)
            fatal("editor exit");
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    DestroyWindow(owner);
    fprintf(logfile, "RESULT %u failures / %u native paste cases\n", failures,
            (unsigned)(sizeof(buttons) / sizeof(buttons[0]) * sizeof(cases) / sizeof(cases[0])));
    fclose(logfile);
    return failures ? 1 : 0;
}

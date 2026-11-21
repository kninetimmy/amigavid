"""Check button access keys through the editor's queued keyboard message path."""

import ctypes as ct
from ctypes import wintypes as w
import hashlib
import platform
import subprocess
import time

from editor import require
from editor_smoke import authored, row_card


def exercise(exe, digest, work, evidence):
    user = ct.WinDLL("user32", use_last_error=True)
    callback = ct.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)

    class GuiThreadInfo(ct.Structure):
        _fields_ = [("cbSize", w.DWORD), ("flags", w.DWORD),
                    *((name, w.HWND) for name in
                      ("active", "focus", "capture", "menu", "move", "caret")),
                    ("rect", w.RECT)]

    def api(name, result, *args):
        method = getattr(user, name)
        method.restype, method.argtypes = result, args
        return method

    enum = api("EnumWindows", w.BOOL, callback, w.LPARAM)
    pid_of = api("GetWindowThreadProcessId", w.DWORD, w.HWND, ct.POINTER(w.DWORD))
    visible = api("IsWindowVisible", w.BOOL, w.HWND)
    cls_of = api("GetClassNameW", ct.c_int, w.HWND, w.LPWSTR, ct.c_int)
    child = api("GetDlgItem", w.HWND, w.HWND, ct.c_int)
    find_child = api("FindWindowExW", w.HWND, w.HWND, w.HWND, w.LPCWSTR, w.LPCWSTR)
    gui_info = api("GetGUIThreadInfo", w.BOOL, w.DWORD, ct.POINTER(GuiThreadInfo))
    send = api("SendMessageTimeoutW", w.LPARAM, w.HWND, w.UINT, w.WPARAM, w.LPARAM,
               w.UINT, w.UINT, ct.c_void_p)
    post = api("PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)

    def checked(result):
        if not result:
            raise ct.WinError(ct.get_last_error())
        return result

    def find(pid, cls):
        matches = []

        @callback
        def visit(hwnd, _):
            target, name = w.DWORD(), ct.create_unicode_buffer(80)
            pid_of(hwnd, ct.byref(target))
            if target.value == pid and visible(hwnd):
                cls_of(hwnd, name, len(name))
                if name.value == cls:
                    matches.append(hwnd)
            return True

        checked(enum(visit, 0))
        return matches[0] if matches else None

    def owned(proc, hwnd):
        require(proc.poll() is None, "editor exited before window message")
        target = w.DWORD()
        checked(pid_of(hwnd, ct.byref(target)))
        require(target.value == proc.pid, "refusing message to a window outside the test editor")
        return hwnd

    def message(proc, hwnd, msg, wp=0, lp=0, queued=False):
        owned(proc, hwnd)
        if queued:
            checked(post(hwnd, msg, wp, lp))
        else:
            checked(send(hwnd, msg, wp, lp, 2, 3000, None))

    def text(proc, hwnd):
        buf = ct.create_unicode_buffer(256)
        message(proc, hwnd, 0x000D, len(buf), ct.addressof(buf))  # WM_GETTEXT
        return buf.value

    def edit(proc, hwnd, value):
        buf = ct.create_unicode_buffer(value)
        message(proc, child(hwnd, 109), 0x000C, 0, ct.addressof(buf))  # WM_SETTEXT

    def until(proc, predicate, description):
        expires = time.monotonic() + 10
        while time.monotonic() < expires:
            require(proc.poll() is None, f"editor exited: {description}")
            if popup := find(proc.pid, "#32770"):
                raise ValueError(f"editor dialog: {text(proc, child(popup, 65535))}")
            if result := predicate():
                return result
            time.sleep(.01)  # Poll observed state; not a fixed synchronization delay.
        raise ValueError(f"did not observe {description}; use an unlocked interactive desktop")

    log = [platform.platform(), f"{digest}  {exe.name}",
           "Queued WM_SYSCHAR (Alt context) to the focused filename edit; Save queued after it."]

    def record(line):
        log.append(line)
        print(line, flush=True)

    for name, second in (("ROWSA.IFF", False), ("ROWSB.IFF", True)):
        (work / name).write_bytes(row_card(second)[0])
    failures = 0
    for key, effect, button, label in (
            ("u", 2, 104, "Slide On Up"), ("d", 3, 112, "Slide On Down"),
            ("e", 10, 119, "Peel Up"), ("a", 11, 120, "Peel Down")):
        with subprocess.Popen([exe, work], cwd=work.parent) as proc:
            try:
                hwnd = until(proc, lambda: find(proc.pid, "AmigaVidEditor"), "main editor window")
                # Setup only: the transition under test is never sent as a command or click.
                edit(proc, hwnd, "ROWSA.IFF")
                message(proc, hwnd, 0x0111, 100)  # WM_COMMAND: Open picture
                message(proc, hwnd, 0x0111, 103)  # initial Straight Cut
                edit(proc, hwnd, "ROWSB.IFF")
                message(proc, hwnd, 0x0111, 100)
                saved = work / f"alt-{key}.vid"
                edit(proc, hwnd, saved.name)

                def focused():
                    info = GuiThreadInfo(cbSize=ct.sizeof(GuiThreadInfo))
                    checked(gui_info(pid_of(hwnd, None), ct.byref(info)))
                    return info.active == hwnd and info.focus == child(hwnd, 109)

                until(proc, focused, "editor thread with filename focus")
                # The real message loop passes this to IsDialogMessageA. A synchronous
                # Save could overtake the queued key, so both are posted in queue order.
                message(proc, child(hwnd, 109), 0x0106, ord(key), 1 | (1 << 29), queued=True)
                message(proc, hwnd, 0x0111, 102, queued=True)  # Save, not the effect
                until(proc, lambda: find_child(hwnd, None, "STATIC", "Project saved."),
                      f"Alt+{key.upper()} project save")
                actual = saved.read_bytes()
                expected = authored("ROWSA.IFF", "ROWSB.IFF", 25, 25, effect)
                for name, data in ((saved.name, actual), (f"alt-{key}.expected.vid", expected)):
                    with (evidence / name).open("xb") as stream:
                        stream.write(data)
                # Fixed two-picture fixture; exact bytes below also check all other fields.
                lines = actual.splitlines()
                count = int(lines[11])
                effects = [int(lines[13 + 10 * i]) for i in range(count)]
                ok = actual == expected
                record(f"{'PASS' if ok else 'FAIL'} Alt+{key.upper()}: saved {count} cues, "
                       f"IDs {effects}; expected exactly [1, {effect}]; "
                       f"SHA-256 {hashlib.sha256(actual).hexdigest()}")

                captions, previous = [], None
                while previous := find_child(hwnd, previous, "BUTTON", None):
                    captions.append(text(proc, previous))
                matches = [caption for caption in captions if f"&{key}" in caption.lower()]
                caption = text(proc, child(hwnd, button))
                unique = matches == [caption] and caption.replace("&", "") == f"Add {label}"
                record(f"{'PASS' if unique else 'FAIL'} Alt+{key.upper()} unique button "
                       f"access key, unchanged wording: {matches!r}")
                failures += not (ok and unique)
                message(proc, hwnd, 0x0010, queued=True)  # WM_CLOSE after successful Save
                require(proc.wait(timeout=10) == 0, "editor did not quit cleanly")
            finally:
                if proc.poll() is None:
                    proc.kill()
    record(f"{'FAIL' if failures else 'PASS'} {4 - failures}/4 shortcut checks; "
           "no global input injection or clipboard access; Windows host evidence only.")
    return "\n".join(log) + "\n", failures

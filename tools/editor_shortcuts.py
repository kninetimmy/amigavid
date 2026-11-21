"""Check button access keys through the editor's queued keyboard message path."""

import ctypes as ct
from ctypes import wintypes as w
import hashlib
import platform
import subprocess
import time

from editor import require
from editor_smoke import authored, row_card


def exercise(exe, digest, work, evidence, record):
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
            result = w.LPARAM()
            checked(send(hwnd, msg, wp, lp, 2, 3000, ct.byref(result)))
            return result.value

    def text(proc, hwnd):
        buf = ct.create_unicode_buffer(256)
        message(proc, hwnd, 0x000D, len(buf), ct.addressof(buf))  # WM_GETTEXT
        return buf.value

    def edit(proc, hwnd, value, control=109):
        buf = ct.create_unicode_buffer(value)
        message(proc, child(hwnd, control), 0x000C, 0, ct.addressof(buf))  # WM_SETTEXT

    def until(proc, predicate, description, required=True):
        expires = time.monotonic() + 10
        while time.monotonic() < expires:
            require(proc.poll() is None, f"editor exited: {description}")
            if popup := find(proc.pid, "#32770"):
                raise ValueError(f"editor dialog: {text(proc, child(popup, 65535))}")
            if result := predicate():
                return result
            time.sleep(.01)  # Poll observed state; not a fixed synchronization delay.
        if required:
            raise ValueError(f"did not observe {description}; use an unlocked interactive desktop")

    def shortcut(proc, hwnd, key):
        def focused():
            info = GuiThreadInfo(cbSize=ct.sizeof(GuiThreadInfo))
            checked(gui_info(pid_of(hwnd, None), ct.byref(info)))
            return info.active == hwnd and info.focus == child(hwnd, 109)

        until(proc, focused, "editor thread with filename focus")
        message(proc, child(hwnd, 109), 0x0106, ord(key), 1 | (1 << 29), queued=True)

    for line in (platform.platform(), f"{digest}  {exe.name}",
                 "Queued WM_SYSCHAR (Alt context) to the focused filename edit; "
                 "no direct command or click for the action under test."):
        record(line)

    def close(proc, hwnd, key):
        try:
            message(proc, hwnd, 0x0010, queued=True)  # WM_CLOSE
            require(proc.wait(timeout=10) == 0, "editor did not quit cleanly")
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            record(f"FAIL Alt+{key.upper()} editor cleanup: {type(error).__name__}: {error}")
            raise

    def binding(proc, hwnd, key, button, label):
        captions, previous = [], None
        while previous := find_child(hwnd, previous, "BUTTON", None):
            captions.append(text(proc, previous))
        matches = [caption for caption in captions if f"&{key}" in caption.lower()]
        caption = text(proc, child(hwnd, button))
        unique = matches == [caption] and caption.replace("&", "") == label
        record(f"{'PASS' if unique else 'FAIL'} Alt+{key.upper()} unique button "
               f"access key, unchanged wording: {matches!r}")
        return unique

    def retain(name, data):
        with (evidence / name).open("xb") as stream:
            stream.write(data)

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

                # The real message loop passes this to IsDialogMessageA. A synchronous
                # Save could overtake the queued key, so both are posted in queue order.
                shortcut(proc, hwnd, key)
                message(proc, hwnd, 0x0111, 102, queued=True)  # Save, not the effect
                until(proc, lambda: find_child(hwnd, None, "STATIC", "Project saved."),
                      f"Alt+{key.upper()} project save")
                actual = saved.read_bytes()
                expected = authored("ROWSA.IFF", "ROWSB.IFF", 25, 25, effect)
                for name, data in ((saved.name, actual), (f"alt-{key}.expected.vid", expected)):
                    retain(name, data)
                # Fixed two-picture fixture; exact bytes below also check all other fields.
                lines = actual.splitlines()
                count = int(lines[11])
                effects = [int(lines[13 + 10 * i]) for i in range(count)]
                ok = actual == expected
                record(f"{'PASS' if ok else 'FAIL'} Alt+{key.upper()}: saved {count} cues, "
                       f"IDs {effects}; expected exactly [1, {effect}]; "
                       f"SHA-256 {hashlib.sha256(actual).hexdigest()}")

                unique = binding(proc, hwnd, key, button, f"Add {label}")
                failures += not (ok and unique)
                close(proc, hwnd, key)
            finally:
                if proc.poll() is None:
                    proc.kill()
    # Long enough to observe real Play clock progress without racing completion.
    prepared = authored("ROWSA.IFF", "ROWSB.IFF", 4000, 25)
    (work / "prepared.vid").write_bytes(prepared)
    retain("prepared.vid", prepared)
    for key, button, label in (("i", 100, "Open picture"), ("p", 105, "Play"),
                                ("s", 102, "Save VID"), ("k", 107, "Seek")):
        with subprocess.Popen([exe, work], cwd=work.parent) as proc:
            try:
                hwnd = until(proc, lambda: find(proc.pid, "AmigaVidEditor"), "main editor window")
                if key != "i":
                    edit(proc, hwnd, "prepared.vid")
                    message(proc, hwnd, 0x0111, 101)  # Setup: Open VID, never the tested action.
                status = find_child(hwnd, child(hwnd, 107), "STATIC", None)
                saved = work / f"alt-{key}.vid"
                # Alt+P on the old build opens this valid picture instead of playing.
                edit(proc, hwnd, saved.name if key == "s" else "ROWSA.IFF")
                if key == "k":
                    edit(proc, hwnd, "00025", 111)
                unique = binding(proc, hwnd, key, button, label)
                shortcut(proc, hwnd, key)

                def result():
                    if key == "i":
                        return message(proc, child(hwnd, 108), 0x0146) == 1  # CB_GETCOUNT
                    if key == "p":
                        return text(proc, status) == "Playing." and int(text(proc, child(hwnd, 111))) > 0
                    if key == "s":
                        return saved.is_file() and text(proc, status) == "Project saved."
                    return text(proc, child(hwnd, 111)) == "25"

                # A missing command is a completed failed case, not a harness abort.
                observed = bool(until(proc, result, f"Alt+{key.upper()} {label}", required=False))
                count = message(proc, child(hwnd, 108), 0x0146)
                state, position = text(proc, status), text(proc, child(hwnd, 111))
                if key == "i":
                    name = ct.create_unicode_buffer(80)
                    if count == 1:
                        message(proc, child(hwnd, 108), 0x0148, 0, ct.addressof(name))  # CB_GETLBTEXT
                    ok = observed and name.value == "6: ROWSA.IFF" and state.startswith("Picture opened.")
                    detail = f"{count} pictures, first entry {name.value!r}, status {state!r}"
                elif key == "s":
                    actual = saved.read_bytes() if saved.is_file() else b""
                    retain("alt-s.actual.vid", actual)
                    retain("alt-s.expected.vid", prepared)
                    ok = observed and actual == prepared
                    detail = f"saved {len(actual)} bytes, SHA-256 {hashlib.sha256(actual).hexdigest()}"
                else:
                    ok = observed and count == 2 and (key == "p" or state == "Project opened.")
                    detail = f"tick {position!r}, {count} pictures, status {state!r}"
                record(f"{'PASS' if ok else 'FAIL'} Alt+{key.upper()} {label}: {detail}")
                failures += not (ok and unique)

                if key == "p":
                    message(proc, hwnd, 0x0111, 106)  # Stop only after observing Play.
                if key == "p" or (key == "i" and observed):
                    # Diagnostic save / clean close; neither is the tested action.
                    if key == "i":
                        message(proc, hwnd, 0x0111, 103)
                    edit(proc, hwnd, saved.name)
                    message(proc, hwnd, 0x0111, 102)
                    retain(saved.name, saved.read_bytes())
                close(proc, hwnd, key)
            finally:
                if proc.poll() is None:
                    proc.kill()
    return failures

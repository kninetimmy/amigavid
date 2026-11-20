"""Drive the native editor controls and compare the actual painted preview."""

from contextlib import contextmanager
import ctypes as ct
from ctypes import wintypes as w
import hashlib
import os
import platform
import struct
import subprocess
import time

from _winapi import CreateJunction
from asset import FIXTURE_PINS, NAMES
from editor import FIXTURE, HDR, require


def authored(first, second, tick, duration):
    names = [bytes(c + 1 for c in ("VCPics:" + path).encode("ascii"))
             for path in (first, second)]
    return (HDR + b"AUTHOR\n1\n2\n" + b"\nZIP\n".join(names) + b"\nZIP\n"
            b"0\n0\n0\n2\n0\n1\n6\n0\n0\n0\n0\n0\n0\nNOTEXT\n" +
            f"{tick}\n2\n7\n{duration}\n0\n0\n0\n0\n0\nNOTEXT\n".encode("ascii"))


def exercise(exe, digest, work, out):
    user = ct.WinDLL("user32", use_last_error=True)
    gdi = ct.WinDLL("gdi32", use_last_error=True)
    callback = ct.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)

    def api(dll, name, result, *args):
        method = getattr(dll, name)
        method.restype, method.argtypes = result, args
        return method

    find_all = api(user, "EnumWindows", w.BOOL, callback, w.LPARAM)
    pid_of = api(user, "GetWindowThreadProcessId", w.DWORD, w.HWND, ct.POINTER(w.DWORD))
    visible = api(user, "IsWindowVisible", w.BOOL, w.HWND)
    cls_of = api(user, "GetClassNameW", ct.c_int, w.HWND, w.LPWSTR, ct.c_int)
    get_child = api(user, "GetDlgItem", w.HWND, w.HWND, ct.c_int)
    post = api(user, "PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
    send = api(user, "SendMessageTimeoutW", w.LPARAM, w.HWND, w.UINT, w.WPARAM, w.LPARAM,
               w.UINT, w.UINT, ct.c_void_p)
    redraw = api(user, "RedrawWindow", w.BOOL, w.HWND, ct.c_void_p, w.HANDLE, w.UINT)
    get_dc = api(user, "GetDC", w.HDC, w.HWND)
    release = api(user, "ReleaseDC", ct.c_int, w.HWND, w.HDC)
    memory = api(gdi, "CreateCompatibleDC", w.HDC, w.HDC)
    delete_dc = api(gdi, "DeleteDC", w.BOOL, w.HDC)
    dib = api(gdi, "CreateDIBSection", w.HBITMAP, w.HDC, ct.c_void_p, w.UINT,
              ct.POINTER(ct.c_void_p), w.HANDLE, w.DWORD)
    select = api(gdi, "SelectObject", w.HANDLE, w.HDC, w.HANDLE)
    delete = api(gdi, "DeleteObject", w.BOOL, w.HANDLE)
    blit = api(gdi, "BitBlt", w.BOOL, w.HDC, ct.c_int, ct.c_int, ct.c_int, ct.c_int, w.HDC,
               ct.c_int, ct.c_int, w.DWORD)
    lock = api(ct.WinDLL("kernel32", use_last_error=True), "CreateFileW", w.HANDLE, w.LPCWSTR,
               w.DWORD, w.DWORD, w.LPVOID, w.DWORD, w.DWORD, w.HANDLE)
    close_handle = api(ct.WinDLL("kernel32", use_last_error=True), "CloseHandle", w.BOOL, w.HANDLE)

    def checked(result):
        if not result:
            raise ct.WinError(ct.get_last_error())
        return result

    def until(proc, predicate, description):
        expires = time.monotonic() + 10
        while time.monotonic() < expires:
            if (code := proc.poll()) is not None:
                raise ValueError(f"editor exited {code}: {description}")
            if result := predicate():
                return result
            time.sleep(.04)
        raise ValueError(f"did not observe {description}; leave editor visible at 100% scale")

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

        checked(find_all(visit, 0))
        return matches[0] if matches else None

    def edit(hwnd, id, text):
        buf = ct.create_unicode_buffer(str(text))
        checked(send(checked(get_child(hwnd, id)), 0x000C, 0, ct.addressof(buf), 2, 1000, None))

    def command(hwnd, id, modal=False):
        child = checked(get_child(hwnd, id))
        if modal:
            checked(post(child, 0x00F5, 0, 0))
        else:
            if not send(child, 0x00F5, 0, 0, 2, 3000, None):
                pid = w.DWORD()
                pid_of(hwnd, ct.byref(pid))
                popup = find(pid.value, "#32770")
                if popup:
                    raise ValueError(dialog_text(popup))
                raise ct.WinError(ct.get_last_error())

    def sync(hwnd):
        checked(send(hwnd, 0, 0, 0, 2, 1000, None))

    def dialog_text(hwnd):
        actual = ct.create_unicode_buffer(512)
        checked(send(checked(get_child(hwnd, 65535)), 0x000D,
                     len(actual), ct.addressof(actual), 2, 1000, None))
        return actual.value

    def dialog(proc, text):
        hwnd = until(proc, lambda: find(proc.pid, "#32770"), "error dialog")
        actual = dialog_text(hwnd)
        require(text in actual, f"wrong error: {actual!r}")
        checked(post(hwnd, 0x0010, 0, 0))
        until(proc, lambda: not visible(hwnd), f"dismissal of {actual!r}")

    @contextmanager
    def launch():
        with subprocess.Popen([exe, work], cwd=out) as proc:
            try:
                hwnd = until(proc, lambda: find(proc.pid, "AmigaVidEditor"), "main editor window")
                yield proc, hwnd
            finally:
                if proc.poll() is None:
                    proc.kill()

    def quit_editor(proc, hwnd):
        checked(post(hwnd, 0x0010, 0, 0))
        require(proc.wait(timeout=10) == 0, "editor did not quit cleanly")

    def frame(hwnd):
        checked(redraw(hwnd, None, None, 0x0105))
        dc = checked(get_dc(hwnd))
        try:
            dest = checked(memory(dc))
            try:
                bits = ct.c_void_p()
                info = struct.pack("<IiiHHIIiiII", 40, 320, -256, 1, 32, 0, 0, 0, 0, 0, 0)
                bitmap = checked(dib(dc, info, 0, ct.byref(bits), None, 0))
                try:
                    old = checked(select(dest, bitmap))
                    try:
                        checked(blit(dest, 0, 0, 320, 256, dc, 0, 0, 0x00CC0020))
                        checked(gdi.GdiFlush())
                        data = ct.string_at(bits, 320 * 256 * 4)
                    finally:
                        select(dest, old)
                finally:
                    delete(bitmap)
            finally:
                delete_dc(dest)
        finally:
            release(hwnd, dc)
        return b"".join(data[i:i + 3][::-1] for i in range(0, len(data), 4))

    def sha(data):
        return hashlib.sha256(data).hexdigest()

    def seek(hwnd, tick):
        edit(hwnd, 111, tick)
        command(hwnd, 107)

    def capture(proc, hwnd, tick, expected_hash):
        seek(hwnd, tick)
        return until(proc, lambda: (data := frame(hwnd)) and sha(data) == expected_hash and data,
                     f"expected full preview at tick {tick}")

    def drag(hwnd, start, end):
        # The timeline initially spans 0..124 across 552 pixels at (16,316).
        coord = lambda tick: (16 + round(tick * 552 / 124)) | (320 << 16)
        for msg, wp, tick in ((0x0201, 1, start), (0x0200, 1, end), (0x0202, 0, end)):
            checked(send(hwnd, msg, wp, coord(tick), 2, 1000, None))

    def open_file(hwnd, name, button, modal=False):
        edit(hwnd, 109, name)
        command(hwnd, button, modal)

    log = [platform.platform(), f"{digest}  {exe.name}"]
    intended = authored(*NAMES, 50, 19)
    project = work / "sequence.vid"
    fixture = work / "fixture.vid"
    (work / "Sub").mkdir()
    (work / "Warm Art.iff").write_bytes((work / NAMES[0]).read_bytes())
    (work / "Sub" / "cool.16").write_bytes((work / NAMES[1]).read_bytes())
    fixture.write_bytes(FIXTURE)
    (work / "invalid.vid").write_bytes(FIXTURE[:-1])
    (work / "missing.vid").write_bytes(FIXTURE.replace(b"WDQjdt;Xbsn!Bsu/jgg", b"WDQjdt;N/27"))
    with unsafe_links(work, out):
        (work / "link.vid").write_bytes(FIXTURE.replace(b"Tvc0dppm/27", b"Mjol0dppm/27"))
        (work / "extra.iff").write_bytes((work / NAMES[0]).read_bytes())
        (work / "hard.iff").hardlink_to(work / "extra.iff")
        with launch() as (proc, hwnd):
            open_file(hwnd, NAMES[0], 100)
            command(hwnd, 103)
            # Capture both original images independently of the transition.
            first = capture(proc, hwnd, 0, FIXTURE_PINS[0][2])
            open_file(hwnd, NAMES[1], 100)
            edit(hwnd, 110, 19)
            command(hwnd, 104)
            drag(hwnd, 25, 50)
            second = capture(proc, hwnd, 79, FIXTURE_PINS[1][2])
            log.append("PASS loaded both pinned ILBMs and authored two cues via buttons and a 25 -> 50 drag.")
            checks = {}
            for tick, split in ((0, 256), (49, 256), (50, 241), (58, 129), (67, 3), (68, 0), (79, 0)):
                expected = first[:split * 960] + second[:(256 - split) * 960]
                checks[tick] = sha(expected)
                capture(proc, hwnd, tick, checks[tick])
                log.append(f"PASS tick {tick} split {split}: {checks[tick]}")
            open_file(hwnd, project.name, 102)
            until(proc, project.is_file, "saved VC10")
            sync(hwnd)
            require(project.read_bytes() == intended, "authored VC10 does not match independent bytes")
            quit_editor(proc, hwnd)

        with launch() as (proc, hwnd):
            open_file(hwnd, fixture.name, 101)
            capture(proc, hwnd, 80, FIXTURE_PINS[0][2])
            roundtrip = work / "roundtrip.vid"
            open_file(hwnd, roundtrip.name, 102)
            until(proc, roundtrip.is_file, "fixture saved as new project")
            require(roundtrip.read_bytes() == FIXTURE, "GUI dropped fixture metadata or pair")
            log.append("PASS independent ordinary/paired fixture opened and saved byte for byte.")
            open_file(hwnd, project.name, 101)
            for tick, expected in checks.items():
                capture(proc, hwnd, tick, expected)
            capture(proc, hwnd, 0, checks[0])
            command(hwnd, 105)
            until(proc, lambda: sha(frame(hwnd)) == FIXTURE_PINS[1][2], "end of timed replay")
            for bad, reason in (("invalid.vid", "Invalid or unsupported"), ("missing.vid", "Cannot open local file"),
                                ("link.vid", "ordinary workspace subfolder")):
                open_file(hwnd, bad, 101, modal=True)
                dialog(proc, reason)
                capture(proc, hwnd, 58, checks[58])
            open_file(hwnd, "hard.iff", 100, modal=True)
            dialog(proc, "not a directory or link")
            capture(proc, hwnd, 58, checks[58])
            log.append("PASS full frame comparison after process restart, timed Play, invalid/missing project and NT link rejection.")
            drag(hwnd, 50, 60)
            # Share reads and writes, but not deletion: staging can succeed,
            # then MoveFile of the previous project must fail.
            handle = lock(str(project), 0x80000000, 3, None, 3, 0, None)
            require(handle not in (None, ct.c_void_p(-1).value), "could not lock project")
            try:
                open_file(hwnd, project.name, 102, modal=True)
                dialog(proc, "Cannot back up previous project")
                require(project.read_bytes() == intended, "failed replacement changed previous bytes")
            finally:
                checked(close_handle(handle))
            open_file(hwnd, project.name, 102)
            updated = authored(*NAMES, 60, 19)
            until(proc, lambda: project.exists() and project.read_bytes() == updated,
                  "successful replacement after unlock")
            sync(hwnd)
            require(not list(work.glob("*.bak")) and not list(work.glob("*.tmp")),
                    "replacement left backup or temporary files")
            backup = project.with_suffix(".vid.bak")
            project.rename(backup)  # Simulate the interruption window before publish.
            open_file(hwnd, project.name, 102, modal=True)
            dialog(proc, "Backup already exists")
            require(not project.exists() and backup.read_bytes() == updated,
                    "retry must not consume an interrupted save's backup")
            backup.rename(project)
            quit_editor(proc, hwnd)
            log.append("PASS locked replacement preserved original bytes; unlocked retry succeeded and cleaned staging.")
            log.append("PASS simulated interrupted replacement: retry left the prior .bak intact for manual recovery.")
    log.append(f"First save SHA-256: {sha(intended)}")
    (out / "HOST-SMOKE.txt").write_text("\n".join(log) + "\n", encoding="utf-8")
    print("\n".join(log))


@contextmanager
def unsafe_links(work, out):
    # Point the junction outside the workspace at a controlled directory.
    # Never reuse an external pathname or remove a target through its link.
    import tempfile
    with tempfile.TemporaryDirectory(dir=out, prefix="Outside workspace ") as folder:
        from pathlib import Path
        outside = Path(folder)
        (outside / "cool.16").write_bytes((work / NAMES[1]).read_bytes())
        link = work / "Link"
        CreateJunction(str(outside), str(link))
        try:
            yield
        finally:
            os.rmdir(link)

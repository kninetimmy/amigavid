"""Exercise the actual Win32 viewer on an unlocked, unscaled Windows desktop."""

from _winapi import CreateJunction
from contextlib import contextmanager
import ctypes as ct
from ctypes import wintypes as w
import hashlib
import os
import platform
import struct
import subprocess
import time
import winreg

from inventory import sha256


TITLES = "AmigaVid ILBM Viewer - "
POINTS = ((0, 0), (244, 14), (140, 110), (203, 215), (319, 255))
# Taken from the independent FFmpeg RGB24 output for the two pinned fixtures.
EXPECTED = (
    ("2b0000", "949494", "2b0000", "2b0000", "2b0000"),
    ("4a4a4a", "4ab2b2", "4ab2b2", "4a6969", "4ab2b2"),
)


def exercise(exe, digest, art, cwd, names, rgb_digests, out):
    user = ct.WinDLL("user32", use_last_error=True)
    gdi = ct.WinDLL("gdi32", use_last_error=True)
    enum_proc = ct.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)

    def bind(dll, name, result, *args):
        fn = getattr(dll, name)
        fn.restype, fn.argtypes = result, args
        return fn

    enum_windows = bind(user, "EnumWindows", w.BOOL, enum_proc, w.LPARAM)
    enum_children = bind(user, "EnumChildWindows", w.BOOL, w.HWND, enum_proc, w.LPARAM)
    window_pid = bind(user, "GetWindowThreadProcessId", w.DWORD, w.HWND, ct.POINTER(w.DWORD))
    visible = bind(user, "IsWindowVisible", w.BOOL, w.HWND)
    title = bind(user, "GetWindowTextW", ct.c_int, w.HWND, w.LPWSTR, ct.c_int)
    class_name = bind(user, "GetClassNameW", ct.c_int, w.HWND, w.LPWSTR, ct.c_int)
    get_menu = bind(user, "GetMenu", w.HMENU, w.HWND)
    submenu = bind(user, "GetSubMenu", w.HMENU, w.HMENU, ct.c_int)
    menu_id = bind(user, "GetMenuItemID", w.UINT, w.HMENU, ct.c_int)
    post = bind(user, "PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
    send = bind(user, "SendMessageTimeoutW", w.LPARAM, w.HWND, w.UINT, w.WPARAM,
                w.LPARAM, w.UINT, w.UINT, ct.c_void_p)
    get_dc = bind(user, "GetDC", w.HDC, w.HWND)
    get_window_dc = bind(user, "GetWindowDC", w.HDC, w.HWND)
    release_dc = bind(user, "ReleaseDC", ct.c_int, w.HWND, w.HDC)
    get_rect = bind(user, "GetWindowRect", w.BOOL, w.HWND, ct.POINTER(w.RECT))
    to_screen = bind(user, "ClientToScreen", w.BOOL, w.HWND, ct.POINTER(w.POINT))
    redraw = bind(user, "RedrawWindow", w.BOOL, w.HWND, ct.c_void_p, w.HANDLE, w.UINT)
    pixel = bind(gdi, "GetPixel", w.COLORREF, w.HDC, ct.c_int, ct.c_int)
    pat_blt = bind(gdi, "PatBlt", w.BOOL, w.HDC, ct.c_int, ct.c_int, ct.c_int, ct.c_int, w.DWORD)
    memory_dc = bind(gdi, "CreateCompatibleDC", w.HDC, w.HDC)
    delete_dc = bind(gdi, "DeleteDC", w.BOOL, w.HDC)
    dib_section = bind(gdi, "CreateDIBSection", w.HBITMAP, w.HDC, ct.c_void_p, w.UINT,
                       ct.POINTER(ct.c_void_p), w.HANDLE, w.DWORD)
    select = bind(gdi, "SelectObject", w.HANDLE, w.HDC, w.HANDLE)
    delete_object = bind(gdi, "DeleteObject", w.BOOL, w.HANDLE)
    bit_blt = bind(gdi, "BitBlt", w.BOOL, w.HDC, ct.c_int, ct.c_int, ct.c_int, ct.c_int, w.HDC,
                   ct.c_int, ct.c_int, w.DWORD)

    def checked(value):
        if not value:
            raise ct.WinError(ct.get_last_error())
        return value

    def wait_for(process, expectation, predicate):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if result := predicate():
                return result
            if (code := process.poll()) is not None:
                raise ValueError(f"viewer exited {code} waiting for {expectation}")
            time.sleep(0.05)
        raise ValueError(f"did not observe {expectation} on the interactive desktop")

    def window_of(process, klass):
        found = []

        @enum_proc
        def collect(hwnd, _):
            pid, text = w.DWORD(), ct.create_unicode_buffer(64)
            window_pid(hwnd, ct.byref(pid))
            if pid.value == process.pid and visible(hwnd):
                class_name(hwnd, text, len(text))
                if text.value == klass:
                    found.append(hwnd)
            return True

        enum_windows(collect, 0)
        return found[0] if found else None

    def caption(hwnd):
        text = ct.create_unicode_buffer(128)
        title(hwnd, text, len(text))
        return text.value

    @contextmanager
    def launch(root):
        with subprocess.Popen([str(exe), str(root)], cwd=cwd) as process:
            try:
                yield process
            finally:
                if process.poll() is None:
                    process.kill()

    @contextmanager
    def dc_of(hwnd, full_window=False):
        dc = checked(get_window_dc(hwnd) if full_window else get_dc(hwnd))
        try:
            yield dc
        finally:
            release_dc(hwnd, dc)

    def samples(hwnd):
        with dc_of(hwnd) as dc:
            values = (pixel(dc, x, y) for x, y in POINTS)
            return tuple(v.to_bytes(4, "little")[:3].hex() for v in values)

    def displayed(process, hwnd, which):
        name = names[which]
        wait_for(process, TITLES + name, lambda: caption(hwnd) == TITLES + name)
        wait_for(process, f"painted {name} pixels", lambda: samples(hwnd) == EXPECTED[which])

    def repaint(process, hwnd, which):
        with dc_of(hwnd) as dc:
            checked(pat_blt(dc, 0, 0, 320, 256, 0x00000042))  # BLACKNESS
        if samples(hwnd) != ("000000",) * len(POINTS):
            raise ValueError("could not black out the displayed sample points")
        checked(redraw(hwnd, None, None, 0x0105))  # INVALIDATE | ERASE | UPDATENOW
        displayed(process, hwnd, which)

    def screenshot(hwnd, which, path):
        # Copy the actual window DC, including its non-client area.
        rect, origin = w.RECT(), w.POINT()
        checked(get_rect(hwnd, ct.byref(rect)))
        checked(to_screen(hwnd, ct.byref(origin)))
        width, height = rect.right - rect.left, rect.bottom - rect.top
        dx, dy = origin.x - rect.left, origin.y - rect.top
        if not (0 <= dx <= width - 320 and 0 <= dy <= height - 256 and width < 4096 and height < 4096):
            raise ValueError("window must fit the 320x256 client area on an unscaled desktop")
        size = width * height * 4
        info = struct.pack("<IiiHHIIiiII", 40, width, -height, 1, 32, 0, size, 0, 0, 0, 0)
        bits = ct.c_void_p()
        with dc_of(hwnd, full_window=True) as window_dc:
            dc = checked(memory_dc(window_dc))
            try:
                bitmap = checked(dib_section(window_dc, info, 0, ct.byref(bits), None, 0))
                try:
                    old = checked(select(dc, bitmap))
                    try:
                        checked(bit_blt(dc, 0, 0, width, height, window_dc, 0, 0, 0x00CC0020))
                        checked(gdi.GdiFlush())
                        data = ct.string_at(bits, size)
                    finally:
                        select(dc, old)
                finally:
                    delete_object(bitmap)
            finally:
                delete_dc(dc)
        rgb = hashlib.sha256()
        for y in range(dy, dy + 256):
            start = 4 * (y * width + dx)
            row = data[start:start + 4 * 320]
            rgb.update(b"".join(row[i:i + 3][::-1] for i in range(0, len(row), 4)))
        if rgb.hexdigest() != rgb_digests[which]:
            raise ValueError("screenshot client RGB24 differed: keep the window visible and unobscured")
        path.write_bytes(struct.pack("<2sIHHI", b"BM", size + 54, 0, 0, 54) + info + data)
        observe(f"Screenshot {path.name} {width}x{height}, SHA-256 {sha256(path)} (local only); "
                f"320x256 client RGB24 SHA-256 {rgb.hexdigest()}")

    def dismiss_error(process, *expected):
        hwnd = wait_for(process, "error dialog", lambda: window_of(process, "#32770"))

        def matching_text():
            strings = []

            @enum_proc
            def collect(child, _):
                text = ct.create_unicode_buffer(256)
                if send(child, 0x000D, len(text), ct.addressof(text), 0x0002, 500, None):
                    strings.append(text.value)
                return True

            enum_children(hwnd, collect, 0)
            text = " ".join(strings)
            return text if all(part in text for part in expected) else None

        text = wait_for(process, " / ".join(expected), matching_text)
        observe(f"Dialog: {text}")
        checked(post(hwnd, 0x0010, 0, 0))
        wait_for(process, "dismissed error dialog", lambda: not visible(hwnd))

    def choose(hwnd, command):
        checked(post(hwnd, 0x0111, command, 0))  # WM_COMMAND from File menu

    def close(process, hwnd):
        checked(post(hwnd, 0x0010, 0, 0))
        if (code := process.wait(timeout=10)) != 0:
            raise ValueError(f"viewer WM_CLOSE returned {code}, expected 0")

    def observe(text):
        print(text)
        log.write(text + "\n")

    with (out / "HOST-SMOKE.txt").open("w", encoding="utf-8") as log:
        version_key = r"SOFTWARE\Microsoft\Windows NT\CurrentVersion"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, version_key) as key:
            build, ubr, edition = (winreg.QueryValueEx(key, value)[0] for value in
                                   ("CurrentBuild", "UBR", "EditionID"))
        observe(f"Host: Windows build {build}.{ubr}, {edition}, {platform.machine()}")
        observe(f"{exe.name} SHA-256: {digest}")
        observe(f"Asset root: {art}; working directory: {cwd}")
        observe(f"Samples at {POINTS}, RGB24:")
        with launch(art) as process:
            hwnd = wait_for(process, "viewer window", lambda: window_of(process, "AmigaVidAsset"))
            menu = checked(submenu(checked(get_menu(hwnd)), 0))
            background, picture = (menu_id(menu, i) for i in range(2))
            if background == picture or 0xFFFFFFFF in (background, picture):
                raise ValueError("missing the two File menu commands")
            for which, capture in enumerate(("background.bmp", "picture.bmp")):
                if which:
                    choose(hwnd, picture)
                displayed(process, hwnd, which)
                observe(f"{names[which]}: {', '.join(samples(hwnd))}")
                screenshot(hwnd, which, out / capture)
                repaint(process, hwnd, which)
                observe(f"{names[which]}: blacked-out pixels restored on repaint")

            replacement = art / names[0]
            original = replacement.read_bytes()
            replacement.unlink()
            choose(hwnd, background)
            dismiss_error(process, names[0], "Missing asset")
            repaint(process, hwnd, 1)
            replacement.write_bytes(b"not an ILBM image")
            choose(hwnd, background)
            dismiss_error(process, names[0], "Expected FORM ILBM")
            repaint(process, hwnd, 1)
            observe("Missing and corrupt opens left the current picture intact on repaint.")

            outside = cwd / names[0]
            outside.write_bytes(original)
            replacement.unlink()
            os.link(outside, replacement)
            choose(hwnd, background)
            dismiss_error(process, names[0], "not a directory or link")
            repaint(process, hwnd, 1)
            replacement.unlink()
            CreateJunction(str(cwd), str(replacement))
            try:
                choose(hwnd, background)
                dismiss_error(process, names[0], "not a directory or link")
                repaint(process, hwnd, 1)
            finally:
                replacement.rmdir()

            replacement.write_bytes(original)
            choose(hwnd, background)
            displayed(process, hwnd, 0)
            observe("Restored valid background opened in the same window after failures.")
            close(process, hwnd)

        outside_root = cwd / "Ordinary Child"
        outside_root.mkdir()
        (outside_root / names[0]).write_bytes(original)
        junction = out / "Junction To Outside"
        CreateJunction(str(cwd), str(junction))
        try:
            with launch(junction / outside_root.name) as process:
                hwnd = wait_for(process, "viewer window", lambda: window_of(process, "AmigaVidAsset"))
                dismiss_error(process, names[0], "not reparse points")
                close(process, hwnd)
        finally:
            junction.rmdir()
        with launch(outside / "Not A Child Directory") as process:
            hwnd = wait_for(process, "viewer window", lambda: window_of(process, "AmigaVidAsset"))
            dismiss_error(process, names[0], "Asset-root components must be directories")
            close(process, hwnd)
        with launch(str(art / ".." / cwd.name)) as process:
            dismiss_error(process, "Invalid asset root")
            if process.wait(timeout=10) != 2:
                raise ValueError("parent traversal in asset root was not rejected")
        observe("PASS (Windows host only)")

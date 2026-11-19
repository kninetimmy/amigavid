"""Exercise the extracted probe on the development host's interactive desktop."""

from contextlib import contextmanager
import ctypes as ct
from ctypes import wintypes as w
import platform
import subprocess
import sys
import time


def exercise(exe, data_file, cwd, log, zip_sha256):
    user = ct.WinDLL("user32", use_last_error=True)
    gdi = ct.WinDLL("gdi32", use_last_error=True)
    callback = ct.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)

    def bind(dll, name, result, *args):
        function = getattr(dll, name)
        function.restype = result
        function.argtypes = args
        return function

    enum_windows = bind(user, "EnumWindows", w.BOOL, callback, w.LPARAM)
    enum_children = bind(user, "EnumChildWindows", w.BOOL, w.HWND, callback, w.LPARAM)
    window_pid = bind(user, "GetWindowThreadProcessId", w.DWORD, w.HWND, ct.POINTER(w.DWORD))
    visible = bind(user, "IsWindowVisible", w.BOOL, w.HWND)
    title = bind(user, "GetWindowTextW", ct.c_int, w.HWND, w.LPWSTR, ct.c_int)
    class_name = bind(user, "GetClassNameW", ct.c_int, w.HWND, w.LPWSTR, ct.c_int)
    post = bind(user, "PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
    send = bind(user, "SendMessageTimeoutW", w.LPARAM, w.HWND, w.UINT, w.WPARAM,
                w.LPARAM, w.UINT, w.UINT, ct.POINTER(ct.c_size_t))
    get_dc = bind(user, "GetDC", w.HDC, w.HWND)
    release_dc = bind(user, "ReleaseDC", ct.c_int, w.HWND, w.HDC)
    pixel = bind(gdi, "GetPixel", w.COLORREF, w.HDC, ct.c_int, ct.c_int)
    caption = "AmigaVid Win32 Probe"

    def wait_for(check, expectation, process):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            value = check()
            if value:
                return value
            code = process.poll()
            if code is not None:
                raise ValueError(f"probe exited {code} waiting for {expectation}")
            time.sleep(0.05)
        raise ValueError(f"did not observe {expectation} on the interactive desktop")

    def windows(process, expected_class):
        found = []

        @callback
        def collect(hwnd, _):
            pid = w.DWORD()
            window_pid(hwnd, ct.byref(pid))
            if pid.value == process.pid and visible(hwnd):
                text = ct.create_unicode_buffer(64)
                class_name(hwnd, text, len(text))
                if text.value == expected_class:
                    found.append(hwnd)
            return True

        enum_windows(collect, 0)
        return found[0] if found else None

    def text_of(hwnd):
        text = ct.create_unicode_buffer(256)
        title(hwnd, text, len(text))
        return text.value

    def command(hwnd, message, wp=0, lp=0):
        if not post(hwnd, message, wp, lp):
            raise ct.WinError(ct.get_last_error())

    @contextmanager
    def launch(*args):
        with subprocess.Popen([str(exe), *args], cwd=cwd) as process:
            try:
                yield process
            finally:
                if process.poll() is None:
                    process.kill()

    def exit_code(process, expected):
        code = process.wait(timeout=10)
        if code != expected:
            raise ValueError(f"probe returned {code}, expected {expected}")

    def picture(hwnd):
        dc = get_dc(hwnd)
        if not dc:
            raise ct.WinError(ct.get_last_error())
        try:
            # COLORREF is 0x00bbggrr. Sample the four quadrants and the cross.
            colors = [pixel(dc, x, y) for x, y in
                      ((28, 28), (124, 28), (28, 124), (124, 124), (76, 76))]
            return colors if colors == [0xFF, 0xFF00, 0xFF0000, 0xFF, 0xFFFFFF] else None
        finally:
            release_dc(hwnd, dc)

    def dialog_text(hwnd):
        strings = []

        @callback
        def collect(child, _):
            buf = ct.create_unicode_buffer(256)
            if send(child, 0x000D, len(buf), ct.addressof(buf), 0x0002, 500, None):
                strings.append(buf.value)
            return True

        enum_children(hwnd, collect, 0)
        return " ".join(strings)

    def check_error(expected_text, expected_code):
        with launch() as process:
            hwnd = wait_for(lambda: windows(process, "#32770"), "error dialog", process)
            wait_for(lambda: expected_text in dialog_text(hwnd), expected_text, process)
            command(hwnd, 0x0010)  # WM_CLOSE on the actual message box
            exit_code(process, expected_code)
            observe(f"{expected_text}: actual dialog, exit {expected_code}")

    def observe(line):
        print(line)
        evidence.write(line + "\n")
        evidence.flush()

    with log.open("w", encoding="utf-8") as evidence:
        observe(f"VCPROBE.ZIP SHA-256: {zip_sha256}")
        observe(f"Host: Windows {sys.getwindowsversion().build}, {platform.machine()}")
        observe(f"Launch: {exe} (working directory: {cwd})")
        with launch() as process:
            hwnd = wait_for(lambda: windows(process, "AmigaVidProbe"), "probe window", process)
            ready = wait_for(lambda: text_of(hwnd) if " - Ready - " in text_of(hwnd) else "",
                             "Ready caption", process)
            audio = ready.split(" - ")[-1]
            if not (audio == "No audio (WinMM waveOut=0)"
                    or (audio.startswith("WinMM waveOut=")
                        and int(audio.partition("=")[2]) > 0)):
                raise ValueError(f"unexpected WinMM status: {audio}")
            observe(ready)
            colors = wait_for(lambda: picture(hwnd), "five GDI pixel colors", process)
            observe("Rendered DIB samples (COLORREF): " + ", ".join(f"{c:06x}" for c in colors))
            for message, wp, lp, state in ((0x0201, 1, 28 | (28 << 16), "Mouse"),
                                           (0x0100, 0x20, 1, "Space")):
                command(hwnd, message, wp, lp)
                expected = f"{caption} - {state} - {audio}"
                wait_for(lambda: text_of(hwnd) == expected, expected, process)
                observe(f"{state}: actual caption update after posted Win32 input message")
            command(hwnd, 0x0100, 0x1B, 1)  # WM_KEYDOWN, VK_ESCAPE
            exit_code(process, 0)
            observe("Esc: exit 0")

        with launch("/noaudio") as process:
            hwnd = wait_for(lambda: windows(process, "AmigaVidProbe"), "probe relaunch", process)
            actual_count = audio.split("=")[-1].rstrip(")")
            expected = f"{caption} - Ready - No audio (simulated; WinMM waveOut={actual_count})"
            wait_for(lambda: text_of(hwnd) == expected, expected, process)
            observe(expected)
            command(hwnd, 0x0010)  # WM_CLOSE (same path as clicking the X)
            exit_code(process, 0)
            observe("WM_CLOSE: exit 0")

        original = data_file.read_bytes()
        data_file.unlink()
        check_error(f"Missing {data_file.name}", 2)
        for label, invalid in (("empty", b""), ("trailing byte", original + b"\0"),
                               ("wrong magic", b"XXX1" + original[4:]),
                               ("bad index", original[:-1] + b"\x04")):
            observe(f"Invalid data case: {label}")
            data_file.write_bytes(invalid)
            check_error(f"Invalid {data_file.name}", 3)
        data_file.write_bytes(original)
        with launch() as process:
            hwnd = wait_for(lambda: windows(process, "AmigaVidProbe"), "restored data launch", process)
            command(hwnd, 0x0010)
            exit_code(process, 0)
            observe("Restored data: relaunched and exited 0")
        observe("PASS (modern host only)")

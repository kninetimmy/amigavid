"""Persist real shortcut comparisons without a native window or clock."""

from contextlib import contextmanager, redirect_stderr, redirect_stdout
import ctypes as ct
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import editor
from editor_smoke import authored


class ShortcutOutputTests(unittest.TestCase):
    def test_failed_comparison_survives_close_timeout(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder).resolve()
            exe = out / editor.EXE
            exe.write_bytes(b"test executable")
            (out / "PE-IMPORTS.txt").write_text(
                f"{editor.sha256(exe)}  {exe.name}\n", encoding="utf-8")
            proc, user = Mock(pid=123), Mock()
            proc.__enter__ = Mock(return_value=proc)
            proc.__exit__ = Mock(return_value=False)
            proc.poll.return_value = None
            proc.wait.side_effect = subprocess.TimeoutExpired(str(exe), 10)
            actual = authored("ROWSA.IFF", "ROWSB.IFF", 25, 25, 3)
            workspaces = []

            def launch(args, **_kwargs):
                workspaces.append(args[1])
                return proc

            def pid_of(_hwnd, target):
                if target is not None:
                    target._obj.value = proc.pid
                return 1

            def class_name(_hwnd, buf, _size):
                buf.value = "AmigaVidEditor"
                return len(buf.value)

            def gui_info(_thread, info):
                info._obj.active, info._obj.focus = 1, 109
                return 1

            def find_child(_hwnd, previous, cls, _title):
                return (104 if not previous else 0) if cls == "BUTTON" else 200

            def send(_hwnd, msg, _wp, lp, *_args):
                if msg == 0x000D:  # WM_GETTEXT: the unique, correctly worded button.
                    buf = ct.create_unicode_buffer("Add Slide On &Up")
                    ct.memmove(lp, buf, ct.sizeof(buf))
                return 1

            def post(_hwnd, msg, wp, _lp):
                if (msg, wp) == (0x0111, 102):  # Save produces a wrong effect.
                    (workspaces[-1] / "alt-u.vid").write_bytes(actual)
                return 1

            user.EnumWindows.side_effect = lambda visit, _: visit(1, 0)
            user.GetWindowThreadProcessId.side_effect = pid_of
            user.GetClassNameW.side_effect = class_name
            user.GetDlgItem.side_effect = lambda _hwnd, control: control
            user.FindWindowExW.side_effect = find_child
            user.GetGUIThreadInfo.side_effect = gui_info
            user.SendMessageTimeoutW.side_effect = send
            user.PostMessageW.side_effect = post
            stdout, stderr = io.StringIO(), io.StringIO()
            with patch("editor.WORK", out), patch("editor.pe_headers"), \
                    patch("editor_shortcuts.ct.WinDLL", return_value=user), \
                    patch("editor_shortcuts.subprocess.Popen", side_effect=launch), \
                    patch("editor_shortcuts.time.monotonic", return_value=0), \
                    patch("editor_shortcuts.time.sleep", side_effect=AssertionError("unexpected poll")), \
                    patch("sys.argv", ["editor.py", "shortcut_test"]), \
                    redirect_stdout(stdout), redirect_stderr(stderr):
                self.assertEqual(editor.main(), 1)

            evidence, = out.glob("shortcuts-*")
            self.assertIn(f"Editor shortcut evidence: {evidence}", stdout.getvalue())
            report = (evidence / "SHORTCUT-TEST.txt").read_text(encoding="utf-8")
            self.assertIn("FAIL Alt+U: saved 2 cues, IDs [1, 3]; expected exactly [1, 2]", report)
            self.assertIn("FAIL Alt+U editor cleanup: TimeoutExpired:", report)
            self.assertIn("FAIL incomplete shortcut run: TimeoutExpired:", report)
            self.assertNotIn("shortcut checks;", report)
            self.assertIn("timed out after 10 seconds", stderr.getvalue())
            self.assertEqual((evidence / "alt-u.vid").read_bytes(), actual)
            self.assertEqual((evidence / "alt-u.expected.vid").read_bytes(),
                             authored("ROWSA.IFF", "ROWSB.IFF", 25, 25, 2))
            self.assertFalse(workspaces[0].exists())
            proc.wait.assert_called_once_with(timeout=10)
            proc.kill.assert_called_once_with()

    def test_workspace_cleanup_failure_prevents_overall_pass(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder).resolve()
            exe = out / editor.EXE
            exe.write_bytes(b"test executable")
            (out / "PE-IMPORTS.txt").write_text(
                f"{editor.sha256(exe)}  {exe.name}\n", encoding="utf-8")
            temporary_directory = tempfile.TemporaryDirectory

            @contextmanager
            def workspace(**kwargs):
                with temporary_directory(**kwargs) as path:
                    yield path
                raise OSError("controlled workspace cleanup failure")

            def exercise(_exe, _digest, _work, _evidence, record):
                record("PASS controlled completed comparisons")
                return 0

            with patch("editor.WORK", out), patch("editor.pe_headers"), \
                    patch("editor_shortcuts.exercise", side_effect=exercise), \
                    patch("editor.tempfile.TemporaryDirectory", side_effect=workspace), \
                    patch("sys.argv", ["editor.py", "shortcut_test"]), \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(editor.main(), 1)

            evidence, = out.glob("shortcuts-*")
            report = (evidence / "SHORTCUT-TEST.txt").read_text(encoding="utf-8")
            self.assertIn("PASS controlled completed comparisons\n", report)
            self.assertIn("FAIL incomplete shortcut run: OSError: "
                          "controlled workspace cleanup failure", report)
            self.assertNotIn("shortcut checks;", report)


if __name__ == "__main__":
    unittest.main()

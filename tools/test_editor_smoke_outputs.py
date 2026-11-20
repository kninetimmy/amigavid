"""Check run isolation without launching an editor or using real assets."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import editor


class SmokeOutputTests(unittest.TestCase):
    def test_failed_evidence_and_legacy_links_survive_the_next_run(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            legacy = root / ".editor"
            legacy.mkdir()
            exe = legacy / editor.EXE
            exe.write_bytes(b"test executable")
            (legacy / "PE-IMPORTS.txt").write_text(
                f"{editor.sha256(exe)}  {exe.name}\n", encoding="utf-8")
            names = ("peel-first-10-1-0-0.rgb", "first-10-1-0.vid")
            for name in names:
                target = root / name
                target.write_bytes(name.encode("ascii"))
                (legacy / name).hardlink_to(target)
            (legacy / "HOST-SMOKE.txt").write_bytes(b"legacy success")
            original_names = {path.name for path in legacy.iterdir()}
            runs = []

            def exercise(exe_arg, _digest, work, evidence):
                self.assertEqual(exe_arg, exe)
                self.assertEqual(work.parent, legacy)
                self.assertTrue(work.is_dir())
                announced = f"Editor smoke evidence: {evidence}" in stdout.getvalue().splitlines()
                runs.append((work, evidence, list(evidence.iterdir()), announced))
                payload = b"partial" if len(runs) == 1 else b"complete"
                for name in names:
                    (evidence / name).write_bytes(payload)
                if len(runs) == 1:
                    raise OSError("controlled failure")
                return "PASS stub\n"

            def contents(evidence):
                return {path.name: path.read_bytes() for path in evidence.iterdir()}

            stdout, stderr = io.StringIO(), io.StringIO()
            with patch("editor.WORK", legacy), patch("editor.pe_headers"), \
                    patch("editor.copy_fixture"), patch("editor_smoke.exercise", exercise), \
                    patch("sys.argv", ["editor.py", "smoke"]), \
                    redirect_stdout(stdout), redirect_stderr(stderr):
                self.assertEqual(editor.main(), 1)
                self.assertIn("editor: controlled failure", stderr.getvalue())
                partial = contents(runs[0][1])
                self.assertEqual(partial, dict.fromkeys(names, b"partial"))
                self.assertEqual(editor.main(), 0)

            first_work, first, _, _ = runs[0]
            second_work, second, _, _ = runs[1]
            self.assertNotEqual(first, second)
            self.assertNotEqual(first_work, second_work)
            for work, evidence, existing, announced in runs:
                self.assertEqual(evidence.parent, legacy)
                self.assertNotEqual(evidence, work)
                self.assertFalse(work.exists())
                self.assertEqual(existing, [])
                self.assertTrue(announced)
            self.assertEqual(contents(first), partial)
            self.assertEqual({path.name for path in second.iterdir()}, {*names, "HOST-SMOKE.txt"})
            for name in names:
                self.assertEqual((second / name).read_bytes(), b"complete")
            self.assertEqual((second / "HOST-SMOKE.txt").read_text(encoding="utf-8"), "PASS stub\n")
            self.assertEqual({path.name for path in legacy.iterdir()},
                             original_names | {first.name, second.name})
            for name in names:
                self.assertTrue((legacy / name).samefile(root / name))
                self.assertEqual((root / name).read_bytes(), name.encode("ascii"))
            self.assertEqual((legacy / "HOST-SMOKE.txt").read_bytes(), b"legacy success")

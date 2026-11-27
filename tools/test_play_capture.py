"""Deterministic timed Play retry checks; no native window or clock required."""

from contextlib import redirect_stderr
import hashlib
import io
import unittest
from unittest.mock import Mock, patch

import editor
from editor_smoke import play_capture


PIXELS = b"expected frame"
EXPECTED = hashlib.sha256(PIXELS).hexdigest()


class PlayCaptureTests(unittest.TestCase):
    def test_three_overshoots_fail_smoke_with_ordered_stop_ticks(self):
        stop = Mock(side_effect=[13, 11, 12])
        frame = Mock()
        errors = io.StringIO()

        def smoke():
            play_capture(10, EXPECTED, stop, frame, [])

        with patch("editor.smoke", smoke), \
                patch("sys.argv", ["editor.py", "smoke"]), redirect_stderr(errors):
            self.assertEqual(editor.main(), 1)
        self.assertIn("editor: timed Play did not stop at tick 10 in three attempts",
                      errors.getvalue())
        self.assertIn("stop ticks by attempt: [13, 11, 12]", errors.getvalue())
        self.assertEqual(stop.call_count, 3)
        frame.assert_not_called()

    def test_wrong_frame_at_target_fails_without_retry(self):
        stop = Mock(side_effect=[10, 10])
        frame = Mock(return_value=b"wrong frame")
        log = []
        with self.assertRaisesRegex(ValueError, "timed Play frame at tick 10"):
            play_capture(10, EXPECTED, stop, frame, log)
        stop.assert_called_once_with()
        frame.assert_called_once_with()
        self.assertEqual(log, [])

    def test_skip_then_correct_frame_passes_and_keeps_skip_log(self):
        stop = Mock(side_effect=[11, 10])
        frame = Mock(return_value=PIXELS)
        log = []
        play_capture(10, EXPECTED, stop, frame, log)
        self.assertEqual(stop.call_count, 2)
        frame.assert_called_once_with()
        self.assertEqual(log, ["RETRY timed Play skipped tick 10, stopped at 11."])

    def test_no_stop_then_correct_frame_passes_and_keeps_retry_log(self):
        stop = Mock(side_effect=[None, 10])

        def capture():
            self.assertEqual(stop.call_count, 2)
            return PIXELS

        frame = Mock(side_effect=capture)
        log = []
        self.assertEqual(play_capture(10, EXPECTED, stop, frame, log), PIXELS)
        self.assertEqual(stop.call_count, 2)
        frame.assert_called_once_with()
        self.assertEqual(log, ["RETRY timed Play requested tick 10, no stop observed."])


if __name__ == "__main__":
    unittest.main()

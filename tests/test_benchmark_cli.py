"""Tests for benchmark command selection without a connected board."""

import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "run_scroll_benchmark.py"


class FakeSerial:
    def __init__(self, *args, **kwargs):
        self.port = None
        self.written = b""
        self.pending = (
            b"Scroll benchmark starting: transport, 40 steps, range 208 px, "
            b"tiles 2, avatar normal, format rgb565, effects normal, touch 10 ms; do not touch\n"
            b"Scroll benchmark per update: prepare 1 ms, draw/other 1 ms, sync 1 ms, total 3 ms\n"
        )
        self.closed = False

    @property
    def in_waiting(self):
        return len(self.pending)

    def open(self):
        pass

    def reset_input_buffer(self):
        pass

    def write(self, data):
        self.written = bytes(data)

    def read(self, count):
        data, self.pending = self.pending[:count], self.pending[count:]
        return data

    def close(self):
        self.closed = True


class BenchmarkCliTest(unittest.TestCase):
    def setUp(self):
        self.connection = FakeSerial()
        serial = types.SimpleNamespace(Serial=lambda **kwargs: self.connection)
        spec = importlib.util.spec_from_file_location("scroll_benchmark", SCRIPT)
        self.module = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {"serial": serial}):
            spec.loader.exec_module(self.module)

    def test_transport_and_touch_commands(self):
        args = ["benchmark", "COM6", "--screen", "transport", "--touch", "10", "--tiles", "2"]
        with patch.object(sys, "argv", args), patch.object(self.module.time, "sleep"), \
                contextlib.redirect_stdout(io.StringIO()):
            result = self.module.main()
        self.assertEqual(result, 0)
        self.assertEqual(self.connection.written, b"2ic")
        self.assertTrue(self.connection.closed)

    def test_transport_rejects_profile_only_options(self):
        args = ["benchmark", "COM6", "--screen", "transport", "--avatar", "hidden"]
        with patch.object(sys, "argv", args), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                self.module.main()


if __name__ == "__main__":
    unittest.main()

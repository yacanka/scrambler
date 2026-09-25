"""Run with: python3 tests/test_console.py /path/to/scrambler"""

import pathlib
import re
import subprocess
import sys
import tempfile
import unittest


EXECUTABLE = str(pathlib.Path(sys.argv.pop(1)).resolve())


class ConsoleTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def run_console(self, *paths, input_text=None):
        result = subprocess.run(
            [EXECUTABLE, *map(str, paths)], input=input_text or "",
            capture_output=True, encoding="utf-8", timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        return result.stdout

    def assert_completed(self, output):
        percentages = [int(value) for value in re.findall(r"%(\d+)", output)]
        self.assertTrue(percentages, "Progress must be reported")
        self.assertEqual(percentages[0], 0)
        self.assertEqual(percentages[-1], 100)
        self.assertEqual(percentages, sorted(percentages))
        self.assertIn("Tamamlandı", output)
        self.assertIn("Tahmini kalan:", output)

    def test_roundtrip_preserves_bytes_and_reports_both_directions(self):
        original = self.root / "archive.zip"
        # Cross progress update boundaries and exercise every byte value.
        payload = b"PK\x03\x04" + bytes(range(256)) * 1024
        original.write_bytes(payload)
        self.assert_completed(self.run_console(original))
        scrambled = self.root / "archive_scrambled.zip"
        self.assertEqual(scrambled.stat().st_size, len(payload))
        self.assertNotEqual(scrambled.read_bytes(), payload)
        self.assert_completed(self.run_console(scrambled))
        restored = self.root / "archive_scrambled_restored.zip"
        self.assertEqual(restored.read_bytes(), payload)
        self.assertEqual(original.read_bytes(), payload)

    def test_empty_file_completes_without_dividing_by_zero(self):
        path = self.root / "empty.bin"
        path.write_bytes(b"")
        self.assert_completed(self.run_console(path))
        self.assertEqual((self.root / "empty_restored.bin").read_bytes(), b"")

    def test_single_byte_rotation_is_preserved(self):
        path = self.root / "one.bin"
        path.write_bytes(b"\x01")
        self.assert_completed(self.run_console(path))
        self.assertEqual((self.root / "one_restored.bin").read_bytes(), b"\x20")

    def test_missing_file_never_reports_completion(self):
        output = self.run_console(self.root / "missing.zip")
        self.assertIn("HATA", output)
        self.assertNotIn("%100", output)
        self.assertNotIn("Tamamlandı", output)

    def test_output_open_failure_never_reports_completion(self):
        path = self.root / "blocked.zip"
        path.write_bytes(b"PK\x03\x04")
        (self.root / "blocked_scrambled.zip").mkdir()
        output = self.run_console(path)
        self.assertIn("HATA", output)
        self.assertNotIn("%100", output)
        self.assertEqual(path.read_bytes(), b"PK\x03\x04")

    def test_interactive_quoted_path_with_spaces(self):
        path = self.root / "my archive.zip"
        path.write_bytes(b"PK\x03\x04")
        output = self.run_console(input_text=f'"{path}"\n\n')
        self.assert_completed(output)
        self.assertTrue((self.root / "my archive_scrambled.zip").exists())

    def test_each_file_has_its_own_progress(self):
        paths = [self.root / "first.zip", self.root / "second.zip"]
        for path in paths:
            path.write_bytes(b"PK\x03\x04")
        output = self.run_console(*paths)
        self.assertEqual(output.count("%100"), 2)
        self.assertEqual(output.count("Tamamlandı"), 2)

    if sys.platform != "win32":
        def test_write_and_close_failures_never_report_completion(self):
            import resource
            import signal

            def limit_output_size():
                signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                resource.setrlimit(resource.RLIMIT_FSIZE, (1024, 1024))

            # The small case fails while flushing fclose; the large one
            # fails during the rotation loop. Neither may report success.
            for size in (2048, 262144):
                with self.subTest(size=size):
                    path = self.root / "limited.zip"
                    payload = b"PK\x03\x04" + b"a" * (size - 4)
                    path.write_bytes(payload)
                    result = subprocess.run(
                        [EXECUTABLE, str(path)], capture_output=True,
                        encoding="utf-8", timeout=30,
                        preexec_fn=limit_output_size,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("HATA", result.stdout)
                    self.assertNotIn("%100", result.stdout)
                    self.assertNotIn("Tamamlandı", result.stdout)
                    self.assertFalse(
                        (self.root / "limited_scrambled.zip").exists()
                    )
                    self.assertEqual(path.read_bytes(), payload)


if __name__ == "__main__":
    unittest.main()

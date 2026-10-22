#!/usr/bin/env python3
import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "apply_v129_russian_polish.py"
spec = importlib.util.spec_from_file_location("patcher", SCRIPT)
patcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patcher)

class PatchTests(unittest.TestCase):
    def test_all_replacements_apply_and_second_run_is_safe(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for filename, pairs in patcher.REPLACEMENTS.items():
                body = "\n".join(old for old, new in pairs) + "\n"
                (root / filename).write_text(body, encoding="utf-8")
            planned = patcher.plan(root)
            for path, body in planned.items():
                for old, new in patcher.REPLACEMENTS[path.name]:
                    self.assertNotIn(old, body)
                    self.assertIn(new, body)
            for path, body in planned.items():
                path.write_text(body, encoding="utf-8")
            again = patcher.plan(root)
            self.assertEqual(planned, again)

    def test_missing_source_fails_without_mutating_other_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            filename, pairs = next(iter(patcher.REPLACEMENTS.items()))
            path = root / filename
            path.write_text("\n".join(old for old, new in pairs), encoding="utf-8")
            before = path.read_bytes()
            with self.assertRaises(SystemExit):
                patcher.plan(root)
            self.assertEqual(before, path.read_bytes())

if __name__ == "__main__":
    unittest.main()

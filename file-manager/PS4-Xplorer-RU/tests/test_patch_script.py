#!/usr/bin/env python3
"""Exercise the patcher with temporary Unity-like fixtures."""
import tempfile
import unittest
from pathlib import Path

from importlib.util import spec_from_file_location, module_from_spec

PATCHER_PATH = Path(__file__).resolve().parents[1] / "apply_russian_patch.py"
SPEC = spec_from_file_location("apply_russian_patch", PATCHER_PATH)
PATCHER = module_from_spec(SPEC)
SPEC.loader.exec_module(PATCHER)

class PatchScriptTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source_root = Path(self.temp.name) / "Assets" / "Codigos"
        self.source_root.mkdir(parents=True)
        (self.source_root / "Controlador.cs").write_text(
            'class Controlador {\n            case SystemLanguage.Spanish:\n}\n', encoding="utf-8"
        )
        (self.source_root / "Opciones.cs").write_text(
            'class Opciones {\n            case SystemLanguage.Spanish:\n}\n', encoding="utf-8"
        )
        (self.source_root / "OpcionesAvanzadas.cs").write_text(
            'class OpcionesAvanzadas {\n    public void OnDisable() {}\n}\n', encoding="utf-8"
        )

    def run_patch(self, *args):
        return PATCHER.main(["--source-root", str(self.source_root), *args])

    def test_dry_run_does_not_change_files(self):
        before = {p.name: p.read_bytes() for p in self.source_root.glob("*.cs")}
        self.run_patch("--dry-run")
        after = {p.name: p.read_bytes() for p in self.source_root.glob("*.cs")}
        self.assertEqual(before, after)

    def test_patch_inserts_translations_and_valid_csharp_escape(self):
        self.run_patch()
        controller = (self.source_root / "Controlador.cs").read_text(encoding="utf-8-sig")
        options = (self.source_root / "Opciones.cs").read_text(encoding="utf-8-sig")
        advanced = (self.source_root / "OpcionesAvanzadas.cs").read_text(encoding="utf-8-sig")
        self.assertIn("Перемещение...", controller)
        self.assertIn('OPTIONS\\nСправка', controller)
        self.assertNotIn('OPTIONS\\\\nСправка', controller)
        self.assertIn("Создать папку", options)
        self.assertIn("FTP-сервер", advanced)
        self.assertIn("Полный доступ (чтение/запись)", advanced)

    def test_second_run_refuses_to_duplicate_localization(self):
        self.run_patch()
        with self.assertRaises(SystemExit):
            self.run_patch()

    def test_preflight_failure_leaves_other_files_untouched(self):
        controller = self.source_root / "Controlador.cs"
        before = controller.read_bytes()
        advanced = self.source_root / "OpcionesAvanzadas.cs"
        advanced.write_text("class Broken {}\n", encoding="utf-8")
        with self.assertRaises(SystemExit):
            self.run_patch()
        self.assertEqual(before, controller.read_bytes())

    def test_missing_source_file_fails_without_partial_writes(self):
        controller = self.source_root / "Controlador.cs"
        before = controller.read_bytes()
        (self.source_root / "Opciones.cs").unlink()
        with self.assertRaises(SystemExit):
            self.run_patch()
        self.assertEqual(before, controller.read_bytes())

if __name__ == "__main__":
    unittest.main()

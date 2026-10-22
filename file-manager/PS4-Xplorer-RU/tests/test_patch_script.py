#!/usr/bin/env python3
"""Exercise the patcher with temporary fixture files, without touching a Unity checkout."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

PATCHER_PATH = Path(__file__).resolve().parents[1] / "apply_russian_patch.py"
SPEC = importlib.util.spec_from_file_location("apply_russian_patch", PATCHER_PATH)
PATCHER = importlib.util.module_from_spec(SPEC)
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
        self.original_root = PATCHER.SOURCE_ROOT
        PATCHER.SOURCE_ROOT = self.source_root
        self.addCleanup(setattr, PATCHER, "SOURCE_ROOT", self.original_root)

    def test_inserts_russian_text_and_newline_escape(self):
        PATCHER.main()
        controller = (self.source_root / "Controlador.cs").read_text(encoding="utf-8-sig")
        options = (self.source_root / "Opciones.cs").read_text(encoding="utf-8-sig")
        advanced = (self.source_root / "OpcionesAvanzadas.cs").read_text(encoding="utf-8-sig")
        self.assertIn("Перемещение...", controller)
        self.assertIn('OPTIONS\\nСправка', controller)
        self.assertNotIn('OPTIONS\\\\nСправка', controller)
        self.assertIn("Создать папку", options)
        self.assertIn("FTP-сервер", advanced)
        self.assertIn("Полный доступ (чтение/запись)", advanced)

    def test_refuses_to_apply_twice(self):
        PATCHER.main()
        with self.assertRaises(SystemExit):
            PATCHER.main()

if __name__ == "__main__":
    unittest.main()

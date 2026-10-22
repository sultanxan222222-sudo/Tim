#!/usr/bin/env python3
"""Validate that the CI-produced patch changes only expected source files."""
import sys
from pathlib import Path

EXPECTED = {
    "Assets/Codigos/Controlador.cs",
    "Assets/Codigos/Opciones.cs",
    "Assets/Codigos/OpcionesAvanzadas.cs",
}
REQUIRED = (
    "Перемещение...",
    "Копирование...",
    "Создать папку",
    "FTP-сервер",
    "Полный доступ (чтение/запись)",
)

def main():
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "ps4-xplorer-ru.patch")
    if not path.is_file() or path.stat().st_size == 0:
        raise SystemExit("FAIL: patch artifact is missing or empty")
    patch = path.read_text(encoding="utf-8", errors="replace")
    changed = {
        line[6:].strip()
        for line in patch.splitlines()
        if line.startswith("diff --git ")
    }
    actual = set()
    for line in patch.splitlines():
        if line.startswith("diff --git a/"):
            actual.add(line.split(" b/", 1)[0][len("diff --git a/"):])
    if actual != EXPECTED:
        raise SystemExit(f"FAIL: expected exactly {sorted(EXPECTED)}, got {sorted(actual)}")
    for term in REQUIRED:
        if term not in patch:
            raise SystemExit(f"FAIL: expected Russian term missing from patch: {term}")
    print("PASS: patch artifact contains all expected Russian source changes and no unrelated files.")
    print("NOTE: source patch validation is not Unity compilation or PS4 PKG validation.")

if __name__ == "__main__":
    main()

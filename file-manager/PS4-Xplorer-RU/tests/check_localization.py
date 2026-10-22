#!/usr/bin/env python3
"""Fast static checks for the partial PS4-Xplorer Russian adaptation."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTROLLER = ROOT / "upstream/Assets/Codigos/Controlador.cs"
OPTIONS = ROOT / "upstream/Assets/Codigos/Opciones.cs"
DICTIONARY = ROOT / "ru-RU.json"

def require(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)

def main():
    controller = CONTROLLER.read_text(encoding="utf-8-sig")
    options = OPTIONS.read_text(encoding="utf-8-sig")
    dictionary = json.loads(DICTIONARY.read_text(encoding="utf-8"))
    require("case SystemLanguage.Russian:" in controller, "Russian controller case missing")
    require("case SystemLanguage.Russian:" in options, "Russian options case missing")
    for label in ("Перемещение...", "Копирование...", "Сначала скопируйте", "Справка"):
        require(label in controller, f"controller translation missing: {label}")
    for label in ("Создать папку", "Вырезать", "Копировать", "Вставить", "Переименовать", "Удалить"):
        require(label in options, f"options translation missing: {label}")
    require("OPTIONS\\nСправка" in controller, "help label must use a C# newline escape")
    require("OPTIONS\\\\nСправка" not in controller, "help label contains a double-escaped newline")
    require(dictionary.get("language") == "Русский", "dictionary language must be Russian")
    menu = dictionary.get("menu", {})
    for key in ("new_folder", "cut", "copy", "paste", "rename", "delete", "help", "operation_failed"):
        require(bool(menu.get(key)), f"dictionary key missing/empty: {key}")
    print("PASS: JSON dictionary and partial C# Russian localization checks passed.")
    print("NOTE: static checks do not compile Unity or prove PKG launchability on PS4.")

if __name__ == "__main__":
    main()

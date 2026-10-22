#!/usr/bin/env python3
"""Safely polish the Russian strings in the complete PS4-Xplorer 1.29 source fork.

This produces source edits only. It does not compile Unity or create a PS4 PKG.
"""
import argparse
import os
import tempfile
from pathlib import Path

DEFAULT_ROOT = Path(__file__).resolve().parent / "PS4-Xplorer-v129" / "Assets" / "Codigos"

REPLACEMENTS = {
    "Controlador.cs": [
        ('CadenaNoHasCopiado = "Вы ничего не выбрали для копирования или вставки";',
         'CadenaNoHasCopiado = "Сначала скопируйте или вырежьте файл для вставки";'),
        ('textoOptions.text = "OPTIONS\\\\nдля инструкций";',
         'textoOptions.text = "OPTIONS\\\\nСправка";'),
        ('CadenaErrorCrearCarpeta = "Папка которую вы пытаетесь создать уже существует";',
         'CadenaErrorCrearCarpeta = "Папка с таким именем уже существует";'),
        ('CadenaRwActivo = "Полные права доступа к системным папкам (только для продвинутых пользователей !)";',
         'CadenaRwActivo = "Полный доступ к чтению и записи системных папок (опасно)";'),
        ('CadenaHomeSalvado = "Этот путь сохранен, как \\'\\'Домашний\\'\\' (Нажмите R3, чтобы вернуться сюда в любое время)";',
         'CadenaHomeSalvado = "Путь сохранён как домашний. Нажмите R3, чтобы вернуться сюда";'),
        ('CadenaExtraer = "экстракт";',
         'CadenaExtraer = "Распаковать";'),
    ],
    "Opciones.cs": [
        ('MisOpciones[0].transform.GetComponentInChildren<Text>().text = "Новая папка";',
         'MisOpciones[0].transform.GetComponentInChildren<Text>().text = "Создать папку";'),
        ('MisOpciones[1].transform.GetComponentInChildren<Text>().text = "Новый файл";',
         'MisOpciones[1].transform.GetComponentInChildren<Text>().text = "Создать файл";'),
        ('MisOpciones[2].transform.GetComponentInChildren<Text>().text = "Скачать URL здесь";',
         'MisOpciones[2].transform.GetComponentInChildren<Text>().text = "Скачать по ссылке";'),
        ('CadenaSeguro = "Уверены ?";', 'CadenaSeguro = "Вы уверены?";'),
    ],
    "OpcionesAvanzadas.cs": [
        ('this.transform.GetChild(0).GetComponentInChildren<Text>().text = "Расширенные Настройки";',
         'this.transform.GetChild(0).GetComponentInChildren<Text>().text = "Расширенные настройки";'),
        ('MisOpciones[1].transform.GetComponentInChildren<Text>().text = "Включить R/W (Опасно!)";',
         'MisOpciones[1].transform.GetComponentInChildren<Text>().text = "Полный доступ (чтение/запись) — опасно";'),
        ('MisOpciones[2].transform.GetComponentInChildren<Text>().text = "Установить домашней";',
         'MisOpciones[2].transform.GetComponentInChildren<Text>().text = "Сделать домашней папкой";'),
    ],
}

def read_text(path):
    with path.open("r", encoding="utf-8-sig", newline="") as f:
        return f.read()

def write_atomic(path, content):
    fd, name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(fd, "w", encoding="utf-8-sig", newline="") as f:
            f.write(content)
        os.replace(name, path)
    except Exception:
        try:
            os.unlink(name)
        except OSError:
            pass
        raise

def plan(root):
    result = {}
    for filename, pairs in REPLACEMENTS.items():
        path = root / filename
        if not path.is_file():
            raise SystemExit(f"Не найден исходный файл: {path}")
        text = read_text(path)
        for old, new in pairs:
            if old not in text:
                if new in text:
                    continue
                raise SystemExit(f"Не найден ожидаемый русский текст в {filename}: {old}")
            if text.count(old) != 1:
                raise SystemExit(f"Ожидалось одно совпадение в {filename}: {old}")
            text = text.replace(old, new, 1)
        result[path] = text
    return result

def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    changes = plan(args.source_root)
    if args.dry_run:
        print(f"PASS: проверены {len(changes)} файлов; изменения не записаны.")
        return 0
    for path, content in changes.items():
        write_atomic(path, content)
        print(f"Обновлён перевод: {path.name}")
    print("Готово: улучшения русских строк применены. Это не сборка PKG.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

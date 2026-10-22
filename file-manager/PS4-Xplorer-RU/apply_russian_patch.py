#!/usr/bin/env python3
"""Safely apply the initial Russian localization to a PS4-Xplorer Unity checkout.

This is a partial source patch, not a PKG builder. Always review the resulting
diff and build with the matching Unity PS4 toolchain before installing anything.
"""
import argparse
import os
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEFAULT_SOURCE_ROOT = ROOT / "PS4-Xplorer" / "Assets" / "Codigos"

CONTROLADOR_CASE = r'''            case SystemLanguage.Russian:
                CadenaMoviendo = "Перемещение...";
                CadenaCopiando = "Копирование...";
                CadenaNoHasCopiado = "Сначала скопируйте или вырежьте файл для вставки";
                CadenaErrorCrearCarpeta = "Папка с таким именем уже существует";
                textoOptions.text = "OPTIONS\nСправка";
                break;
'''

OPCIONES_CASE = '''            case SystemLanguage.Russian:
                MisOpciones[0].transform.GetComponentInChildren<Text>().text = "Создать папку";
                MisOpciones[1].transform.GetComponentInChildren<Text>().text = "Вырезать";
                MisOpciones[2].transform.GetComponentInChildren<Text>().text = "Копировать";
                MisOpciones[3].transform.GetComponentInChildren<Text>().text = "Вставить";
                MisOpciones[4].transform.GetComponentInChildren<Text>().text = "Переименовать";
                CadenaEliminar = "Удалить";
                MisOpciones[5].transform.GetComponentInChildren<Text>().text = CadenaEliminar;
                CadenaSeguro = "Вы уверены?";
                break;
'''

ADVANCED_BLOCK = '''    public void Awake()
    {
        if (Application.systemLanguage != SystemLanguage.Russian || MisOpciones == null)
            return;

        if (MisOpciones.Length > 0 && MisOpciones[0] != null)
        {
            var label = MisOpciones[0].GetComponentInChildren<UnityEngine.UI.Text>();
            if (label != null)
                label.text = "FTP-сервер";
        }

        if (MisOpciones.Length > 1 && MisOpciones[1] != null)
        {
            var label = MisOpciones[1].GetComponentInChildren<UnityEngine.UI.Text>();
            if (label != null)
                label.text = "Полный доступ (чтение/запись)";
        }
    }

'''

def read_text(path: Path) -> str:
    # newline="" preserves the source file's existing LF/CRLF convention.
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return stream.read()

def write_atomic(path: Path, content: str) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(fd, "w", encoding="utf-8-sig", newline="") as stream:
            stream.write(content)
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except OSError:
            pass
        raise

def plan_case(path: Path, block: str) -> str:
    if not path.is_file():
        raise SystemExit(f"Не найден исходный файл: {path}")
    source = read_text(path)
    if "case SystemLanguage.Russian:" in source:
        raise SystemExit(f"В {path} уже есть русский case. Ничего не изменено; проверьте файл вручную.")
    anchor = "            case SystemLanguage.Spanish:"
    if source.count(anchor) != 1:
        raise SystemExit(f"Ожидался ровно один блок Spanish в {path}; найдено: {source.count(anchor)}. Ничего не изменено.")
    newline = "\r\n" if "\r\n" in source else "\n"
    block = block.replace("\n", newline)
    return source.replace(anchor, block + anchor, 1)

def plan_advanced(path: Path) -> str:
    if not path.is_file():
        raise SystemExit(f"Не найден исходный файл: {path}")
    source = read_text(path)
    has_ftp = "FTP-сервер" in source
    has_rw = "Полный доступ (чтение/запись)" in source
    has_awake = "public void Awake()" in source
    if has_ftp or has_rw or has_awake:
        raise SystemExit(f"В {path} уже есть Awake/русские подписи. Ничего не изменено; проверьте файл вручную.")
    anchor = "    public void OnDisable()"
    if source.count(anchor) != 1:
        raise SystemExit(f"Ожидался ровно один OnDisable в {path}; найдено: {source.count(anchor)}. Ничего не изменено.")
    newline = "\r\n" if "\r\n" in source else "\n"
    block = ADVANCED_BLOCK.replace("\n", newline)
    return source.replace(anchor, block + anchor, 1)

def build_plan(source_root: Path):
    targets = {
        source_root / "Controlador.cs": plan_case(source_root / "Controlador.cs", CONTROLADOR_CASE),
        source_root / "Opciones.cs": plan_case(source_root / "Opciones.cs", OPCIONES_CASE),
        source_root / "OpcionesAvanzadas.cs": plan_advanced(source_root / "OpcionesAvanzadas.cs"),
    }
    return targets

def main(argv=None):
    parser = argparse.ArgumentParser(description="Добавить частичную русификацию PS4-Xplorer в копию Unity-исходников.")
    parser.add_argument("--source-root", type=Path, default=DEFAULT_SOURCE_ROOT,
                        help="путь к каталогу Assets/Codigos исходного Unity-проекта")
    parser.add_argument("--dry-run", action="store_true", help="проверить совместимость файлов, ничего не записывая")
    args = parser.parse_args(argv)
    source_root = args.source_root.resolve()
    plan = build_plan(source_root)  # preflight all files before writing any of them
    if args.dry_run:
        print(f"DRY RUN: проверены {len(plan)} файлов; изменения не записаны.")
        return 0
    for path, updated in plan.items():
        write_atomic(path, updated)
    print(f"Готово: обновлены {len(plan)} файла(ов) в {source_root}.")
    print("Это частичная локализация, не полная сборка и не PKG. Проверьте diff и соберите в Unity.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Apply the initial Russian-language additions to a PS4-Xplorer Unity source checkout.

This intentionally refuses to overwrite an already translated source file. The
upstream public source may not match the latest 2.08 binary, so always review
the diff and build/test with the matching Unity PS4 project before packaging.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE_ROOT = ROOT / "PS4-Xplorer" / "Assets" / "Codigos"

CONTROLADOR_CASE = '''            case SystemLanguage.Russian:
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

def insert_case(path: Path, block: str) -> None:
    if not path.is_file():
        raise SystemExit(f"Не найден исходный файл: {path}")
    text = path.read_text(encoding="utf-8-sig")
    if "case SystemLanguage.Russian:" in text:
        raise SystemExit(f"Русская локализация уже есть в {path}; файл не изменён.")
    anchor = "            case SystemLanguage.Spanish:"
    if anchor not in text:
        raise SystemExit(f"Не найден ожидаемый блок Spanish в {path}; файл не изменён.")
    text = text.replace(anchor, block + anchor, 1)
    path.write_text(text, encoding="utf-8-sig", newline="")

def main():
    insert_case(SOURCE_ROOT / "Controlador.cs", CONTROLADOR_CASE)
    insert_case(SOURCE_ROOT / "Opciones.cs", OPCIONES_CASE)
    print("Добавлены начальные русские переводы в Controlador.cs и Opciones.cs.")
    print("Это не полная локализация: проверьте остальные экраны и соберите проект в совместимой Unity PS4-среде.")

if __name__ == "__main__":
    main()

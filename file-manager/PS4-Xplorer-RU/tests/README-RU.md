# Проверки

Из корня репозитория выполните:

```sh
python -m py_compile file-manager/PS4-Xplorer-RU/apply_russian_patch.py
python file-manager/PS4-Xplorer-RU/tests/check_localization.py
python file-manager/PS4-Xplorer-RU/tests/test_patch_script.py
```

`check_localization.py` проверяет строки в сохранённых копиях исходных C#-скриптов и структуру JSON-словаря.

`test_patch_script.py` использует временные тестовые файлы и проверяет dry-run, вставку русских строк, защиту от повторного применения и остановку до частичных изменений при ошибке предварительной проверки.

Эти проверки не заменяют компиляцию Unity, сборку PKG или тестирование на PS4.

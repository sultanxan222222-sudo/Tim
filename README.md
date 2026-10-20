# TIM · PS4 Game Catalog

Проект `Tim` — карточный каталог для PS4. Текущий репозиторий содержит веб-прототип интерфейса, а не нативный пакет PS4.

## Подключённые источники

- PSPrices — бесплатные игры PS4: https://psprices.com/region-ru/collection/free?platform=PS4
- PlayStation Store — бесплатные игры: https://store.playstation.com/ru-ru/category/30749d2c-738f-4c36-9d6b-7ed517cca9ee/
- PirateGames — каталог PS4: https://pirategames.ru/ps4/
- PlayStation Store — Rocket League: https://store.playstation.com/ru-ru/search/rocket%20league
- PlayStation Store — Brawlhalla: https://store.playstation.com/ru-ru/search/brawlhalla

## Текущее состояние

- Карточки, обложки, поиск, фильтры, сортировка и подробности.
- `catalog.json` хранит источники и записи; ссылки источников показываются в интерфейсе.
- Можно подключить внешний JSON-каталог в формате проекта или формате FPKGi `DATA`.
- Кнопка скачивания показывается только для записей с прямым URL пакета.
- Добавление PirateGames пока означает добавление ссылки как источника поиска/метаданных. Автоматический сбор карточек с сайта и проверка его ссылок ещё не реализованы.
- Страница каталога не равна прямой ссылке на PKG. Необходимо получать метаданные и прямые ссылки из источников, которые разрешают соответствующее использование; не подставлять выдуманные URL.
- Это веб-прототип, а не устанавливаемый PS4 PKG. Нативная очередь загрузок, фоновая загрузка и установка на консоли ещё не реализованы.

## Формат каталога

```json
{
  "sources": [
    {
      "id": "homebrew",
      "name": "Разрешённый источник пакетов",
      "url": "https://example.org/catalog.json",
      "type": "json"
    }
  ],
  "games": [
    {
      "id": "example-homebrew",
      "title": "Example Homebrew",
      "titleId": "ABCD12345",
      "category": "Homebrew",
      "sourceId": "homebrew",
      "region": "EUR",
      "version": "1.00",
      "size": 100000000,
      "coverImage": "https://example.org/cover.png",
      "downloadUrl": "https://example.org/authorized-package.pkg",
      "notes": "Пример структуры; URL условные."
    }
  ]
}
```

Размер указывается в байтах. Используй пакеты, которые разрешено распространять или которыми владеешь.

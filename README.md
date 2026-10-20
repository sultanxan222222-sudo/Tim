# TIM · PS4 Game Catalog

Проект `Tim` — карточный каталог для PS4. Текущий репозиторий содержит веб-прототип интерфейса, а не нативный пакет PS4.

## Подключённые каталоги

- PSPrices: бесплатные игры PS4 — https://psprices.com/region-ru/collection/free?platform=PS4
- PlayStation Store: категория бесплатных игр — https://store.playstation.com/ru-ru/category/30749d2c-738f-4c36-9d6b-7ed517cca9ee/
- PlayStation Store: поиск Rocket League — https://store.playstation.com/ru-ru/search/rocket%20league
- PlayStation Store: поиск Brawlhalla — https://store.playstation.com/ru-ru/search/brawlhalla

## Текущее состояние

- Карточки игр, обложки, поиск, фильтры, сортировка и окно подробностей.
- Каталог `catalog.json` содержит записи игр и список источников; источники отображаются в интерфейсе.
- Можно подключить внешний JSON-каталог в формате проекта или формате FPKGi `DATA`.
- Кнопка скачивания PKG должна появляться только у записи с прямым URL пакета.
- Ссылки PSPrices и PlayStation Store — страницы поиска/каталога, а не прямые URL файлов PKG. По ним нельзя достоверно получить прямую ссылку на PKG одним лишь добавлением URL в JSON.
- Автоматический сбор карточек из страниц магазина ещё не реализован: страницы магазинов не предоставляют этому статическому сайту универсальный JSON/API с разрешённым браузерным доступом (CORS).
- Прямые URL пакетов, размер, Title ID и версия должны поступать из разрешённого источника метаданных. Нельзя подставлять выдуманные URL.
- Это пока веб-прототип, не устанавливаемый PS4 PKG; фоновая очередь загрузок и установка на консоли не реализованы.

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

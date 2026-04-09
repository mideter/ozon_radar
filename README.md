# OzonRadar (Qt)

Парсер товаров Ozon: **загрузка страницы** выполняется скриптом Python (`scripts/ozon_fetch.py`) с **undetected-chromedriver** (в stdout — NDJSON с `outerHTML` плиток и URL); **разбор названия, цены и баллов**, фильтрация, топ‑50 и интерфейс — на **C++/Qt Quick** (`ProductCardParser`).

## Зависимости (Linux)

```bash
# Сборка Qt (без WebEngine)
sudo apt install qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev

# QML‑модули для запуска (обязательно, иначе «module QtQuick is not installed»)
sudo apt install qml-module-qtquick2 qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2

# Python: загрузка страницы Ozon
sudo apt install python3 python3-undetected-chromedriver chromium chromium-driver

# При ошибке "QQml_colorProvider" или "Unable to assign undefined to QColor":
sudo apt install qt5-quick-demos
```

## Скрипт загрузки

По умолчанию ищется файл `scripts/ozon_fetch.py` относительно каталога исполняемого файла (`build/../scripts/ozon_fetch.py`) или текущего каталога.

Переопределение пути:

```bash
export OZON_FETCH_SCRIPT=/полный/путь/к/ozon_fetch.py
./ozon_radar
```

## Сборка

```bash
mkdir build && cd build
cmake ..
make
```

## Запуск

Из каталога `build` (чтобы находился `../scripts/ozon_fetch.py`):

```bash
./ozon_radar
```

Или задайте `OZON_FETCH_SCRIPT`.

## Ошибка «module QtQuick is not installed»

На машине не установлены **runtime**‑пакеты QML (сборочных `-dev` недостаточно для запуска). Установите:

```bash
sudo apt install qml-module-qtquick2 qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2
```

## Возможности

- Несколько ссылок в интерфейсе: по одной URL в строке; для **каждой** строки выполняется отдельный запуск `ozon_fetch.py` (новый Chromium), результаты объединяются в один список с дедупликацией товаров
- Загрузка страниц категорий и поиска Ozon через Python + undetected-chromedriver
- Автоматический скролл; плитки передаются как HTML; разбор полей карточки в C++
- Фильтрация по баллам за отзыв (мин/макс) на стороне C++
- Топ-50 товаров по соотношению баллы/цена
- Обновление таблицы в реальном времени

# scripts / ozon_fetch

`ozon_fetch.py` повторяет цикл парсинга из `ozon_cpp/ozon_py` (`PageLoader`, `ElementFinder`, `Scroller`, `ProductProcessor`, `ProductParser`), но драйвер — **undetected-chromedriver**, вывод для Qt — NDJSON `batch` / `done` / `error`.

```bash
pip install -r requirements.txt
python3 ozon_fetch.py "https://www.ozon.ru/..."
```

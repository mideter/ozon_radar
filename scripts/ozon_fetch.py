#!/usr/bin/env python3
"""
CLI для Qt: undetected-chromedriver, карточки как outerHTML в NDJSON.
Stdout: {"type":"batch","items":[{"url","html"},...]}, затем {"type":"done"}.
Парсинг name/price/review_points — в C++ (ProductCardParser).
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any, Dict, List, Set

from chrome_driver_version import detect_chrome_major_version
from element_finder import ElementFinder
from page_loader import PageLoader
from scroller import Scroller
from tile_snapshot import tile_html_and_url


def emit_error(msg: str) -> None:
    print(json.dumps({"type": "error", "message": msg}, ensure_ascii=False), flush=True)
    sys.exit(1)


def emit_batch(items: List[Dict[str, Any]]) -> None:
    if not items:
        return
    print(
        json.dumps({"type": "batch", "items": items}, ensure_ascii=False),
        flush=True,
    )


def collect_new_tiles(
    driver,
    finder: ElementFinder,
    seen_urls: Set[str],
) -> List[Dict[str, str]]:
    """Собирает плитки с новыми URL; дедуп по seen_urls (мутирует seen_urls)."""
    out: List[Dict[str, str]] = []
    for el in finder.find_product_elements():
        snap = tile_html_and_url(driver, el)
        if not snap:
            continue
        u = snap["url"]
        if u in seen_urls:
            continue
        seen_urls.add(u)
        out.append({"url": u, "html": snap["html"]})
    return out


def run(url: str) -> None:
    try:
        import undetected_chromedriver as uc
    except ImportError:
        emit_error(
            "Не установлен undetected-chromedriver. "
            "Установите: sudo apt install python3-undetected-chromedriver"
        )

    options = uc.ChromeOptions()
    maj = detect_chrome_major_version()
    driver = (
        uc.Chrome(options=options, version_main=maj)
        if maj is not None
        else uc.Chrome(options=options)
    )
    try:
        driver.set_window_size(960, 720)

        page_loader = PageLoader(driver)
        page_loader.load(url)

        time.sleep(0.5)
        title = driver.title or ""
        body_text = ""
        try:
            body = driver.find_element("tag name", "body")
            body_text = (body.text or "")[:500]
        except Exception:
            pass
        if "Доступ ограничен" in title or "доступ ограничен" in body_text.lower():
            emit_error(
                "Ozon ограничивает доступ с вашей сети. Попробуйте: отключить VPN, "
                "подключиться к другой сети (Wi‑Fi/мобильный интернет) или перезагрузить роутер."
            )

        time.sleep(2)

        if not page_loader.wait_for_products():
            emit_error("Товары не найдены. Проверьте URL и структуру страницы Ozon.")

        finder = ElementFinder(driver)
        scroller = Scroller(driver)
        seen_urls: Set[str] = set()

        new_items = collect_new_tiles(driver, finder, seen_urls)
        emit_batch(new_items)

        last_height = scroller.get_page_height()

        while True:
            new_height, _ = scroller.scroll_and_wait(last_height)

            new_items = collect_new_tiles(driver, finder, seen_urls)
            emit_batch(new_items)

            if new_height == last_height:
                if new_items:
                    last_height = new_height
                    continue
                break

            last_height = new_height

        print(json.dumps({"type": "done"}, ensure_ascii=False), flush=True)
    finally:
        driver.quit()


def main() -> int:
    argp = argparse.ArgumentParser(description="Ozon fetch для Qt (NDJSON в stdout)")
    argp.add_argument("url", help="URL категории или поиска Ozon")
    args = argp.parse_args()
    try:
        run(args.url)
    except BrokenPipeError:
        return 0
    except SystemExit:
        raise
    except Exception as e:
        emit_error(str(e))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

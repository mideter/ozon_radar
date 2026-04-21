#!/usr/bin/env python3
"""
CLI для Qt: undetected-chromedriver, карточки как outerHTML в NDJSON.
Stdout: {"type":"batch","items":[{"url","html"},...]}, затем {"type":"done"}.
Парсинг name/price/review_points — в C++ (ProductCardParser).
"""
from __future__ import annotations

import argparse
import json
import signal
import sys
import time
from typing import Any, Dict, List, Set

from chrome_driver_version import detect_chrome_major_version
from element_finder import ElementFinder
from page_loader import PageLoader
from scroller import Scroller
from tile_snapshot import tile_html_and_url

import undetected_chromedriver as uc

_stop_requested = False


def _request_stop(*_: object) -> None:
    global _stop_requested
    _stop_requested = True


def _ensure_not_stopped() -> None:
    if _stop_requested:
        raise KeyboardInterrupt


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


def emit_progress(processed: int, total: int) -> None:
    if total <= 1:
        return
    print(
        json.dumps(
            {"type": "progress", "processed_urls": processed, "total_urls": total},
            ensure_ascii=False,
        ),
        flush=True,
    )


def collect_new_tiles(
    driver,
    finder: ElementFinder,
    seen_urls: Set[str],
) -> List[Dict[str, str]]:
    """Собирает плитки с новыми URL; дедуп по seen_urls (мутирует seen_urls)."""
    _ensure_not_stopped()
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


def scrape_one_listing_url(driver, url: str) -> None:
    """Одна страница выдачи: загрузка, скролл, батчи в stdout. Драйвер не закрывается."""
    _ensure_not_stopped()
    page_loader = PageLoader(driver)
    page_loader.load(url)

    time.sleep(0.5)
    finder = ElementFinder(driver)
    scroller = Scroller(driver)
    seen_urls: Set[str] = set()

    new_items = collect_new_tiles(driver, finder, seen_urls)
    emit_batch(new_items)

    last_height = scroller.get_page_height()

    while True:
        _ensure_not_stopped()
        new_height, _ = scroller.scroll_and_wait(last_height)
        new_items = collect_new_tiles(driver, finder, seen_urls)
        emit_batch(new_items)

        if new_height == last_height:
            if new_items:
                last_height = new_height
                continue
            break
        
        last_height = new_height


def run(urls: List[str]) -> None:
    options = uc.ChromeOptions()
    maj = detect_chrome_major_version()
    
    driver = (
        uc.Chrome(options=options, version_main=maj)
        if maj is not None
        else uc.Chrome(options=options)
    )

    try:
        try:
            driver.minimize_window()
        except Exception as ex:
            pass

        total_urls = len(urls)

        for idx, url in enumerate(urls, start=1):
            _ensure_not_stopped()
            scrape_one_listing_url(driver, url)
            emit_progress(idx, total_urls)

        print(json.dumps({"type": "done"}, ensure_ascii=False), flush=True)
    finally:
        driver.quit()


def main() -> int:
    signal.signal(signal.SIGTERM, _request_stop)
    signal.signal(signal.SIGINT, _request_stop)

    argp = argparse.ArgumentParser(description="Ozon fetch для Qt (NDJSON в stdout)")
    argp.add_argument(
        "urls",
        nargs="+",
        help="Один или несколько URL категории или поиска Ozon (один браузер на всю сессию)",
    )
    args = argp.parse_args()
    
    if not args.urls:
        emit_error("Не переданы URL.")

    try:
        run(args.urls)
    except KeyboardInterrupt:
        return 0
    except BrokenPipeError:
        return 0
    except SystemExit:
        raise
    except Exception as e:
        emit_error(str(e))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""
CLI для Qt: undetected-chromedriver + парсинг как в ozon_cpp/ozon_py (OzonScraper.scrape).
Stdout: NDJSON {"type":"batch","items":[...]}, затем {"type":"done"}.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from typing import List

from element_finder import ElementFinder
from page_loader import PageLoader
from product import Product
from product_parser import ProductParser
from product_processor import ProductProcessor
from scroller import Scroller


def emit_error(msg: str) -> None:
    print(json.dumps({"type": "error", "message": msg}, ensure_ascii=False), flush=True)
    sys.exit(1)


def emit_batch(items: List[dict]) -> None:
    if not items:
        return
    print(
        json.dumps({"type": "batch", "items": items}, ensure_ascii=False),
        flush=True,
    )


def products_to_dicts(slice_products: List[Product]) -> List[dict]:
    return [
        {
            "name": p.name,
            "price": p.price,
            "review_points": p.review_points,
            "url": p.url,
        }
        for p in slice_products
    ]


def run(url: str, headless: bool) -> None:
    try:
        import undetected_chromedriver as uc
    except ImportError:
        emit_error(
            "Не установлен undetected-chromedriver. "
            "Установите: sudo apt install python3-undetected-chromedriver"
        )

    options = uc.ChromeOptions()
    if headless:
        options.add_argument("--headless=new")

    driver = uc.Chrome(options=options)
    try:
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
        parser = ProductParser()
        processor = ProductProcessor(parser)

        products: List[Product] = []
        seen_urls: set = set()
        index = 1

        product_elements = finder.find_product_elements()
        if product_elements:
            index = processor.process_products(
                product_elements, seen_urls, products, index
            )
            emit_batch(products_to_dicts(products))

        last_height = scroller.get_page_height()

        while True:
            new_height, _ = scroller.scroll_and_wait(last_height)

            previous_count = len(products)
            product_elements = finder.find_product_elements()
            if product_elements:
                index = processor.process_products(
                    product_elements, seen_urls, products, index
                )
                if len(products) > previous_count:
                    emit_batch(products_to_dicts(products[previous_count:]))

            if new_height == last_height:
                if len(products) > previous_count:
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
    argp.add_argument("--headless", action="store_true", help="Headless Chromium")
    args = argp.parse_args()
    try:
        run(args.url, args.headless)
    except BrokenPipeError:
        return 0
    except SystemExit:
        raise
    except Exception as e:
        emit_error(str(e))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

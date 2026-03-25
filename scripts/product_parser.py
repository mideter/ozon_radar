"""Модуль для парсинга свойств товаров с Ozon (как в ozon_cpp/ozon_py)."""
import re
from typing import Optional

from selenium.webdriver.common.by import By

from product import Product


class ProductParser:
    """Класс для извлечения и парсинга свойств товаров из HTML элементов."""

    def parse_product(
        self,
        element,
        index: int,
        cached_link=None,
        cached_text=None,
    ) -> Optional[Product]:
        try:
            if cached_text is None:
                cached_text = self._get_cached_text(element, None)

            name = self.extract_product_name(element, cached_link, cached_text)
            if not name or len(name) < 3:
                return None

            price = self.extract_product_price(element, cached_text)
            review_points = self.extract_review_points(element, cached_text)
            url = self.extract_product_url(element, cached_link)

            return Product(
                index=index,
                name=name,
                price=price,
                review_points=review_points,
                url=url,
            )
        except Exception:
            return None

    def extract_product_name(self, element, cached_link=None, cached_text=None) -> str:
        try:
            name_elem = element.find_element(
                By.CSS_SELECTOR, "a[target='_blank'] > div > span"
            )
            name_text = name_elem.text.strip()
            if name_text:
                return name_text
        except Exception:
            pass

        try:
            tag = (element.tag_name or "").lower()
            if tag == "a":
                for sel in (
                    "span.tsBody500Medium",
                    "span[class*='tsBody']",
                    "div span",
                    "span",
                ):
                    try:
                        el = element.find_element(By.CSS_SELECTOR, sel)
                        t = (el.text or "").strip()
                        if t and len(t) >= 3:
                            return t
                    except Exception:
                        continue
        except Exception:
            pass

        ct = cached_text or ""
        if ct:
            for line in ct.splitlines():
                line = line.strip()
                if len(line) >= 3:
                    return line
        return ""

    def _get_cached_text(self, element, cached_text):
        if cached_text is None:
            try:
                return element.text.strip() if hasattr(element, "text") else ""
            except Exception:
                return ""
        return cached_text or ""

    def extract_product_price(self, element, cached_text=None) -> int:
        try:
            price_elem = element.find_element(
                By.CSS_SELECTOR,
                "div.tile-root[data-index] > div > div > div > span",
            )
            price_text = price_elem.text.strip()
            if not price_text:
                return 0
            matches = re.findall(r"(\d+(?:\s+\d+)*)", price_text)
            if matches:
                price_str = (
                    matches[-1].replace(" ", "").replace(",", "").replace("\u2009", "")
                )
                return int(price_str)
            return 0
        except Exception:
            pass

        text = cached_text or ""
        if hasattr(element, "text"):
            try:
                text = (element.text or text or "").strip()
            except Exception:
                pass
        if text:
            matches = re.findall(r"(?:₽|руб\\.?|\\b)(?:\\s*)(\\d+(?:\\s+\\d+)*)", text, re.I)
            if not matches:
                matches = re.findall(r"(\\d+(?:\\s+\\d+)*)\\s*₽", text)
            if not matches:
                matches = re.findall(r"(\\d+(?:\\s+\\d+)*)", text)
            if matches:
                price_str = (
                    matches[-1].replace(" ", "").replace(",", "").replace("\u2009", "")
                )
                try:
                    return int(price_str)
                except ValueError:
                    return 0
        return 0

    def extract_review_points(self, element, cached_text=None) -> int:
        try:
            points_elem = element.find_element(
                By.CSS_SELECTOR, "div.tile-root[data-index] section div[title]"
            )
            points_text = points_elem.text.strip()
            if not points_text:
                return 0
            match = re.search(r"(\d+(?:\s+\d+)*)", points_text)
            if match:
                num_str = (
                    match.group(1)
                    .replace(" ", "")
                    .replace(",", "")
                    .replace("\u2009", "")
                )
                return int(num_str) if num_str else 0
            return 0
        except Exception:
            return 0

    def extract_product_url(self, element, cached_link=None) -> str:
        if cached_link is not None:
            try:
                href = cached_link.get_attribute("href")
                if href and "/product/" in href:
                    clean_url = href.split("?")[0].split("#")[0].rstrip("/")
                    if not any(
                        x in clean_url
                        for x in (
                            "/reviews",
                            "/questions",
                            "/seller",
                            "/seller-info",
                        )
                    ):
                        return clean_url
            except Exception:
                pass

        try:
            link_elem = element.find_element(
                By.CSS_SELECTOR, "div.tile-root[data-index] > a"
            )
            href = link_elem.get_attribute("href")
            if href and "/product/" in href:
                clean_url = href.split("?")[0].split("#")[0].rstrip("/")
                if not any(
                    x in clean_url
                    for x in (
                        "/reviews",
                        "/questions",
                        "/seller",
                        "/seller-info",
                    )
                ):
                    return clean_url
        except Exception:
            pass

        return ""

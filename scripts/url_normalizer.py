"""Модуль для нормализации и валидации URL товаров (как в ozon_radar/ozon_py)."""
from typing import Optional, Tuple


class URLNormalizer:
    """Класс для нормализации и валидации URL товаров."""

    EXCLUDED_PATHS = ["/reviews", "/questions", "/seller", "/seller-info"]

    @staticmethod
    def normalize(href: str) -> str:
        if not href:
            return ""
        return href.split("?")[0].split("#")[0].rstrip("/")

    @classmethod
    def is_valid(cls, url: str) -> bool:
        if not url or "/product/" not in url:
            return False
        if any(excluded in url for excluded in cls.EXCLUDED_PATHS):
            return False
        product_id = url.split("/product/")[-1].split("/")[0]
        return len(product_id) >= 3

    @classmethod
    def extract_and_normalize(cls, element) -> Tuple[Optional[str], Optional[object]]:
        from selenium.webdriver.common.by import By

        try:
            if element.tag_name == "a":
                href = element.get_attribute("href")
                if href:
                    normalized = cls.normalize(href)
                    if cls.is_valid(normalized):
                        return normalized, element

            try:
                link_elem = element.find_element(
                    By.CSS_SELECTOR, "div.tile-root[data-index] > a"
                )
                href = link_elem.get_attribute("href")
                if href:
                    normalized = cls.normalize(href)
                    if cls.is_valid(normalized):
                        return normalized, link_elem
            except Exception:
                pass

            try:
                link_elem = element.find_element(
                    By.CSS_SELECTOR, "> a[href*='/product/']"
                )
                href = link_elem.get_attribute("href")
                if href:
                    normalized = cls.normalize(href)
                    if cls.is_valid(normalized):
                        return normalized, link_elem
            except Exception:
                pass

            link_elem = element.find_element(
                By.CSS_SELECTOR, "a[href*='/product/']"
            )
            href = link_elem.get_attribute("href")
            if href:
                normalized = cls.normalize(href)
                if cls.is_valid(normalized):
                    return normalized, link_elem
        except Exception:
            pass

        return None, None

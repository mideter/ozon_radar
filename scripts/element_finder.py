"""Модуль для поиска элементов товаров на странице (как в ozon_cpp/ozon_py)."""
from typing import List

from selenium.webdriver.common.by import By


class ElementFinder:
    """Класс для поиска элементов товаров на странице."""

    def __init__(self, driver):
        self.driver = driver

    def find_product_elements(self) -> List:
        product_elements = []

        try:
            tile_grids = self.driver.find_elements(
                By.CSS_SELECTOR,
                'div[data-widget="tileGridDesktop"], div[data-widget="tileGridMobile"]',
            )

            for grid in tile_grids:
                try:
                    products = grid.find_elements(
                        By.CSS_SELECTOR, "div.tile-root[data-index]"
                    )
                    if not products:
                        # data-index иногда убирают — берём плитки со ссылкой на карточку товара
                        all_roots = grid.find_elements(
                            By.CSS_SELECTOR, "div.tile-root"
                        )
                        products = [
                            el
                            for el in all_roots
                            if el.find_elements(
                                By.CSS_SELECTOR, 'a[href*="/product/"]'
                            )
                        ]
                    product_elements.extend(products)
                except Exception:
                    continue

            if not product_elements:
                product_elements = self.driver.find_elements(
                    By.CSS_SELECTOR, "div.tile-root[data-index]"
                )
            if not product_elements:
                roots = self.driver.find_elements(
                    By.CSS_SELECTOR, "div.tile-root"
                )
                product_elements = [
                    el
                    for el in roots
                    if el.find_elements(By.CSS_SELECTOR, 'a[href*="/product/"]')
                ]
        except Exception:
            try:
                product_elements = self.driver.find_elements(
                    By.CSS_SELECTOR, "div.tile-root[data-index]"
                )
            except Exception:
                product_elements = []

        if not product_elements:
            product_elements = self._links_as_product_elements()

        return product_elements

    def _links_as_product_elements(self) -> List:
        """Если класс плитки сменился — собираем уникальные карточки по ссылкам в ленте."""
        try:
            candidates = self.driver.find_elements(
                By.CSS_SELECTOR,
                '#contentScrollPaginator a[href*="/product/"], '
                'div[data-widget="tileGridDesktop"] a[href*="/product/"], '
                'div[data-widget="tileGridMobile"] a[href*="/product/"]',
            )
        except Exception:
            return []

        seen: set = set()
        out: List = []
        skip = ("/reviews", "/questions", "/seller", "/seller-info")
        for a in candidates:
            try:
                href = a.get_attribute("href") or ""
                if "/product/" not in href:
                    continue
                if any(s in href for s in skip):
                    continue
                key = href.split("?")[0].split("#")[0].rstrip("/")
                if not key or key in seen:
                    continue
                pid = key.split("/product/")[-1].split("/")[0]
                if len(pid) < 3:
                    continue
                seen.add(key)
                out.append(a)
            except Exception:
                continue
        return out


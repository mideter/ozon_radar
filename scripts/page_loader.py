"""Модуль для загрузки страниц и ожидания элементов (ozon_radar/ozon_py + запасной JS-поллинг)."""
import time

from selenium.common.exceptions import TimeoutException
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait

PAGE_LOAD_DELAY = 0.15

# Ozon часто меняет хэш-классы (pi5_24 и т.д.); ждём плитки, сетку виджетов или ссылки на /product/.
# В execute_script тело подставляется в function(){ ... } — нужен явный return, иначе в Python придёт None.
JS_WAIT_PRODUCTS = """
return (function() {
    function gridHasProductLinks(grid) {
        return grid && grid.querySelectorAll('a[href*="/product/"]').length > 0;
    }
    if (document.querySelectorAll('div.tile-root[data-index]').length > 0) return true;
    var roots = document.querySelectorAll('div.tile-root');
    for (var i = 0; i < roots.length; i++) {
        if (roots[i].querySelector('a[href*="/product/"]')) return true;
    }
    var grids = document.querySelectorAll(
        'div[data-widget="tileGridDesktop"], div[data-widget="tileGridMobile"]'
    );
    for (var j = 0; j < grids.length; j++) {
        if (gridHasProductLinks(grids[j])) return true;
    }
    var paginator = document.getElementById('contentScrollPaginator');
    if (paginator) {
        if (paginator.querySelectorAll('a[href*="/product/"]').length > 0) return true;
        if (paginator.querySelectorAll('div.pi5_24').length > 0) return true;
        if (paginator.querySelectorAll('div.tile-root').length > 0) return true;
    }
    return document.querySelectorAll('a[href*="/product/"]').length >= 1;
})()
"""


class PageLoader:
    """Класс для загрузки страниц и ожидания элементов."""

    PAGE_LOAD_DELAY = PAGE_LOAD_DELAY

    def __init__(self, driver):
        self.driver = driver

    def load(self, url: str) -> None:
        self.driver.get(url)
        try:
            WebDriverWait(self.driver, 5).until(
                lambda d: d.execute_script("return document.readyState") == "complete"
            )
            time.sleep(self.PAGE_LOAD_DELAY)
        except TimeoutException:
            time.sleep(self.PAGE_LOAD_DELAY)

    def wait_for_products(self) -> bool:
        """Ждём появления карточек/ссылок на товары без привязки к устаревшим классам (pi5_24)."""
        try:
            WebDriverWait(self.driver, 25).until(
                lambda d: bool(d.execute_script(JS_WAIT_PRODUCTS))
            )
            return True
        except TimeoutException:
            pass

        for _ in range(40):
            try:
                if self.driver.execute_script(JS_WAIT_PRODUCTS):
                    return True
            except Exception:
                pass
            time.sleep(0.5)
        return False

"""Модуль для загрузки страниц и ожидания элементов (ozon_radar/ozon_py + запасной JS-поллинг)."""
import time

from selenium.common.exceptions import TimeoutException
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait

PAGE_LOAD_DELAY = 0.15

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


"""Модуль для управления прокруткой страницы (как в ozon_radar/ozon_py)."""
import time
from typing import Tuple


class Scroller:
    """Класс для управления прокруткой страницы."""

    def __init__(self, driver):
        self.driver = driver

    def scroll_and_wait(self, last_height: int) -> Tuple[int, float]:
        self.driver.execute_script(
            "window.scrollTo(0, document.body.scrollHeight);"
        )
        wait_start = time.time()
        time.sleep(0.1)
        new_height = self.driver.execute_script("return document.body.scrollHeight")

        if new_height == last_height:
            time.sleep(0.5)
            new_height = self.driver.execute_script(
                "return document.body.scrollHeight"
            )

        wait_time = time.time() - wait_start
        return new_height, wait_time

    def get_page_height(self) -> int:
        return self.driver.execute_script("return document.body.scrollHeight")

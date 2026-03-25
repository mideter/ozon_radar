"""Модуль для представления товара."""
from dataclasses import dataclass


@dataclass
class Product:
    """Класс для представления товара."""

    index: int
    name: str
    price: int
    review_points: int = 0
    url: str = ""

    def __post_init__(self) -> None:
        if len(self.name) > 80:
            self.name = self.name[:77] + "..."

    def __hash__(self) -> int:
        return hash(self.url)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Product):
            return self.url == other.url
        return False

    def _extract_points_value(self) -> int:
        return self.review_points

    def get_points_to_price_ratio(self) -> float:
        if self.price <= 0:
            return 0.0
        return self.review_points / self.price

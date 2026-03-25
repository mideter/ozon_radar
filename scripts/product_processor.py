"""Модуль для обработки и парсинга товаров (как в ozon_cpp/ozon_py; без print в stdout)."""
from typing import List, Optional, Set, Tuple

from product import Product
from product_parser import ProductParser
from url_normalizer import URLNormalizer


class ProductProcessor:
    """Класс для обработки и парсинга товаров."""

    def __init__(self, product_parser: ProductParser):
        self.product_parser = product_parser
        self.url_normalizer = URLNormalizer()

    def process_single_product(
        self, element, seen_urls: Set[str], index: int
    ) -> Tuple[Optional[Product], Optional[str]]:
        normalized_url, cached_link = self.url_normalizer.extract_and_normalize(
            element
        )

        if normalized_url and normalized_url in seen_urls:
            return None, None

        cached_text = (
            getattr(element, "text", "").strip() if hasattr(element, "text") else ""
        )
        product = self.product_parser.parse_product(
            element, index, cached_link, cached_text
        )

        if product:
            if normalized_url:
                seen_urls.add(normalized_url)
            return product, normalized_url

        return None, None

    def process_products(
        self,
        product_elements: List,
        seen_urls: Set[str],
        products: List[Product],
        index: int,
    ) -> int:
        for element in product_elements:
            try:
                product, normalized_url = self.process_single_product(
                    element, seen_urls, index
                )

                if product:
                    products.append(product)
                    index += 1
            except Exception:
                pass

        return index

#include "ozon_scraper/batchproductmapper.h"

#include "ozon_scraper/productcardparser.h"

#include <optional>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


QVector<Product> BatchProductMapper::mapBatchJson(const QByteArray& batchJson, int startIndex)
{
    QVector<Product> products;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(batchJson, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return products;

    const QJsonArray items = document.array();
    int index = startIndex;

    for (const QJsonValue& itemValue : items) {
        const QJsonObject item = itemValue.toObject();
        const QString url = item.value("url").toString();
        const QString html = item.value("html").toString();
        const std::optional<ParsedTile> parsedTile = ParsedTile::parseHtml(html);

        if (!parsedTile.has_value())
            continue;
        if (parsedTile->reviewPoints <= 0)
            continue;

        QString shortName = parsedTile->name.trimmed();
        if (shortName.length() > 80)
            shortName = shortName.left(77) + QStringLiteral("...");

        products.append(
            Product(index++, shortName, parsedTile->price, parsedTile->reviewPoints, url));
    }

    return products;
}

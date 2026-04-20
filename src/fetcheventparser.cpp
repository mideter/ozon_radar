#include "fetcheventparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


FetchEvent FetchEventParser::parseLine(const QByteArray& line)
{
    FetchEvent event;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return event;

    const QJsonObject object = document.object();
    const QString type = object.value("type").toString();

    if (type == QStringLiteral("batch")) {
        const QJsonArray items = object.value("items").toArray();
        event.type = FetchEvent::Type::Batch;
        event.batchJson = QJsonDocument(items).toJson(QJsonDocument::Compact);
        return event;
    }

    if (type == QStringLiteral("progress")) {
        event.type = FetchEvent::Type::Progress;
        event.processedUrls = object.value("processed_urls").toInt();
        event.totalUrls = object.value("total_urls").toInt();
        return event;
    }

    if (type == QStringLiteral("error")) {
        event.type = FetchEvent::Type::Error;
        event.errorMessage = object.value("message").toString();
        return event;
    }

    if (type == QStringLiteral("done"))
        event.type = FetchEvent::Type::Done;

    return event;
}

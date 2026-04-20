#pragma once

#include <QByteArray>
#include <QString>


struct FetchEvent
{
    enum class Type {
        Invalid,
        Batch,
        Progress,
        Error,
        Done
    };

    Type type = Type::Invalid;
    QByteArray batchJson;
    int processedUrls = 0;
    int totalUrls = 0;
    QString errorMessage;
};


class FetchEventParser
{
public:
    static FetchEvent parseLine(const QByteArray& line);
};

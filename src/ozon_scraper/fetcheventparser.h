#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>


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
    QVector<FetchEvent> parseChunk(const QByteArray& chunk);

private:
    static FetchEvent parseLine(const QByteArray& line);

    QByteArray chunkBuffer_;
};

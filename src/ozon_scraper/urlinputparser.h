#pragma once

#include <QString>
#include <QStringList>


class UrlInputParser
{
public:
    static QStringList parseMultiline(const QString& text);
};

#include "ozon_scraper/urlinputparser.h"

#include <QUrl>
#include <stdexcept>


QStringList UrlInputParser::parseMultiline(const QString& text)
{
    QStringList out;
    const QStringList rawLines = text.split(QChar('\n'));

    for (QString line : rawLines) {
        line = line.trimmed();

        if (line.isEmpty())
            continue;

        if (!line.startsWith("http://", Qt::CaseInsensitive)
            && !line.startsWith("https://", Qt::CaseInsensitive)) {
            line = "https://" + line;
        }

        const QUrl url = QUrl::fromUserInput(line);
        if (url.isValid() && !url.scheme().isEmpty())
            out.append(url.toString());
    }

    if (out.isEmpty())
        throw std::runtime_error("Некорректные URL. Укажите по одной ссылке в строке.");

    return out;
}

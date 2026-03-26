#include "productcardparser.h"

#include <QRegularExpression>

namespace {

QString stripTags(const QString& html)
{
    QString s = html;
    s.replace(
        QRegularExpression(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption),
        QChar('\n'));
    static const QRegularExpression reScript(
        QStringLiteral("<script[^>]*>[\\s\\S]*?</script>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reStyle(
        QStringLiteral("<style[^>]*>[\\s\\S]*?</style>"),
        QRegularExpression::CaseInsensitiveOption);
    s.remove(reScript);
    s.remove(reStyle);
    static const QRegularExpression reTag(QStringLiteral("<[^>]+>"));
    s.replace(reTag, QStringLiteral(" "));
    s.replace(QLatin1String("&nbsp;"), QChar(0x00A0));
    s.replace(QLatin1String("&amp;"), QLatin1String("&"));
    s.replace(QLatin1String("&lt;"), QLatin1String("<"));
    s.replace(QLatin1String("&gt;"), QLatin1String(">"));
    s.replace(QLatin1String("&quot;"), QLatin1String("\""));
    s.replace(QLatin1String("&#x20bd;"), QStringLiteral("₽"));
    s.replace(QLatin1String("&rub;"), QStringLiteral("₽"));
    return s.simplified();
}

int parseDigitsGroup(const QString& numPart)
{
    QString d = numPart;
    d.remove(QChar(0x00A0));
    d.remove(QChar(0x2009));
    d.remove(QLatin1Char(' '));
    d.remove(QLatin1Char(','));
    d.remove(QLatin1Char('\''));
    bool ok = false;
    const int v = d.toInt(&ok);
    return ok ? v : 0;
}

int extractPriceFromPlain(const QString& text)
{
    // Сначала «27 800 ₽». Не использовать (?:₽|руб|\\b)+цифры: \\b цепляется к первому числу («800» из «800 баллов»).
    static const QRegularExpression reRubAfter(
        QStringLiteral(u"(\\d(?:[\\s\u00a0\u2009]*\\d)*)\\s*₽"));
    static const QRegularExpression reRubSignBeforeDigits(
        QStringLiteral(
            u"(?:₽|руб\\.?)(?:\\s*)(\\d(?:[\\s\u00a0\u2009]*\\d)*)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reDigits(
        QStringLiteral(u"(\\d(?:[\\s\u00a0\u2009]*\\d)*)"));

    QRegularExpressionMatch m = reRubAfter.match(text);
    if (m.hasMatch())
        return parseDigitsGroup(m.captured(1));

    m = reRubSignBeforeDigits.match(text);
    if (m.hasMatch())
        return parseDigitsGroup(m.captured(1));

    int lastPrice = 0;
    auto it = reDigits.globalMatch(text);
    while (it.hasNext()) {
        const QString cap = it.next().captured(1);
        const int v = parseDigitsGroup(cap);
        if (v > lastPrice)
            lastPrice = v;
    }
    return lastPrice;
}

int extractReviewPointsFromHtml(const QString& html)
{
    static const QRegularExpression reSectionDivTitle(
        QStringLiteral(
            "<div[^>]*\\btitle\\s*=\\s*[\"'][^\"']*[\"'][^>]*>([^<]{1,64})</div>"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = reSectionDivTitle.globalMatch(html);
    while (it.hasNext()) {
        const QString chunk = it.next().captured(1).simplified();
        static const QRegularExpression reNum(QStringLiteral("(\\d(?:[\\s\u00a0]*\\d)*)"));
        const QRegularExpressionMatch nm = reNum.match(chunk);
        if (nm.hasMatch())
            return parseDigitsGroup(nm.captured(1));
    }
    return 0;
}

QString extractNameFromHtml(const QString& html, const QString& plain)
{
    static const QRegularExpression reBlankTarget(
        QStringLiteral(
            "<a[^>]*\\btarget\\s*=\\s*[\"']_blank[\"'][^>]*>\\s*"
            "<div[^>]*>\\s*<span[^>]*>([^<]+)</span>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch m = reBlankTarget.match(html);
    if (m.hasMatch()) {
        const QString n = m.captured(1).simplified();
        if (n.length() >= 3)
            return n;
    }

    static const QRegularExpression reTsBody(
        QStringLiteral(
            "<span[^>]*\\bclass\\s*=\\s*[\"'][^\"']*tsBody[^\"']*[\"'][^>]*>([^<]+)</span>"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = reTsBody.globalMatch(html);
    while (it.hasNext()) {
        const QString n = it.next().captured(1).simplified();
        if (n.length() >= 3)
            return n;
    }

    const QStringList lines = plain.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QString t = line.simplified();
        if (t.length() >= 3)
            return t.length() > 200 ? t.left(197) + QStringLiteral("...") : t;
    }

    return {};
}

} // namespace

std::optional<ParsedTile> parseOzonTileHtml(const QString& html, const QString& url)
{
    Q_UNUSED(url);
    if (html.length() < 20)
        return std::nullopt;

    const QString plain = stripTags(html);
    QString name = extractNameFromHtml(html, plain);
    if (name.length() < 3)
        return std::nullopt;

    const int price = extractPriceFromPlain(plain);
    int points = extractReviewPointsFromHtml(html);
    if (points <= 0) {
        static const QRegularExpression rePointsSection(
            QStringLiteral("<section[^>]*>([\\s\\S]{0,4000}?)</section>"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch sm = rePointsSection.match(html);
        if (sm.hasMatch()) {
            const QString secPlain = stripTags(sm.captured(1));
            static const QRegularExpression reNum(QStringLiteral("(\\d(?:[\\s\u00a0]*\\d)*)"));
            const QRegularExpressionMatch nm = reNum.match(secPlain);
            if (nm.hasMatch())
                points = parseDigitsGroup(nm.captured(1));
        }
    }

    ParsedTile t;
    t.name = name;
    t.price = price;
    t.reviewPoints = points;
    return t;
}

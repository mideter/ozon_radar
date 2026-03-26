#include "ozonscraper.h"
#include "productcardparser.h"

#include <algorithm>
#include <optional>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString normalizeUrl(const QString& href)
{
    if (href.isEmpty())
        return {};
    QString u = href.split(QLatin1Char('?')).first().split(QLatin1Char('#')).first();
    while (u.endsWith(QLatin1Char('/')))
        u.chop(1);
    return u;
}

bool isValidProductUrl(const QString& url)
{
    if (url.isEmpty() || !url.contains(QLatin1String("/product/")))
        return false;
    if (url.contains(QLatin1String("/reviews")) || url.contains(QLatin1String("/questions"))
        || url.contains(QLatin1String("/seller")))
        return false;
    const int idx = url.indexOf(QLatin1String("/product/")) + 9;
    const QString id = url.mid(idx).split(QLatin1Char('/')).first();
    return id.length() >= 3;
}

} // namespace

OzonScraper::OzonScraper(QObject* parent)
    : QObject(parent)
    , process_(new QProcess(this))
{
    connect(process_, &QProcess::readyReadStandardOutput, this, &OzonScraper::onProcessStdout);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OzonScraper::onProcessFinished);
}

OzonScraper::~OzonScraper()
{
    stop();
}

QString OzonScraper::resolveFetchScriptPath() const
{
    const QByteArray env = qgetenv("OZON_FETCH_SCRIPT");
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env);

    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = QDir(appDir).filePath(QStringLiteral("../scripts/ozon_fetch.py"));
    if (QFileInfo::exists(p))
        return QDir::cleanPath(p);
    p = QDir::currentPath() + QStringLiteral("/scripts/ozon_fetch.py");
    if (QFileInfo::exists(p))
        return p;
    return QDir::cleanPath(QDir(appDir).filePath(QStringLiteral("../scripts/ozon_fetch.py")));
}

void OzonScraper::start(const QString& urlStr, int minPoints, int maxPoints)
{
    const QUrl url = QUrl::fromUserInput(urlStr);
    if (url.isValid())
        start(url, minPoints, maxPoints);
    else
        emit finishedWithError(QStringLiteral("Некорректный URL."));
}

void OzonScraper::start(const QUrl& url, int minPoints, int maxPoints)
{
    if (running_)
        return;

    const QString scriptPath = resolveFetchScriptPath();
    if (!QFileInfo::exists(scriptPath)) {
        emit finishedWithError(
            QStringLiteral("Не найден скрипт ozon_fetch.py. Укажите OZON_FETCH_SCRIPT или положите "
                           "scripts/ozon_fetch.py рядом с приложением."));
        return;
    }

    url_ = url;
    minPoints_ = minPoints;
    maxPoints_ = maxPoints;
    seenUrls_.clear();
    allProducts_.clear();
    lastTableCount_ = 0;
    lastPrice_ = 0;
    stdoutBuffer_.clear();
    running_ = true;
    elapsedTimer_.start();

    emit statusChanged(QStringLiteral("Загрузка страницы (Python)..."), -1, 0);

    const QString pythonExe = qEnvironmentVariable("OZON_PYTHON", QStringLiteral("python3"));
    const QStringList args{scriptPath, url_.toString()};
    process_->start(pythonExe, args);
    if (!process_->waitForStarted(5000)) {
        running_ = false;
        emit finishedWithError(
            QStringLiteral("Не удалось запустить Python (%1). Установите Python 3 и зависимости "
                           "(см. README).")
                .arg(pythonExe));
    }
}

void OzonScraper::stop()
{
    running_ = false;
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(3000);
    }
}

void OzonScraper::onProcessStdout()
{
    appendStdout(process_->readAllStandardOutput());
}

void OzonScraper::appendStdout(const QByteArray& chunk)
{
    if (chunk.isEmpty())
        return;
    stdoutBuffer_.append(chunk);
    int pos = 0;
    while ((pos = stdoutBuffer_.indexOf('\n')) >= 0) {
        QByteArray line = stdoutBuffer_.left(pos).trimmed();
        stdoutBuffer_.remove(0, pos + 1);
        if (!line.isEmpty())
            handleJsonLine(line);
    }
}

void OzonScraper::handleJsonLine(const QByteArray& line)
{
    if (!running_)
        return;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonObject o = doc.object();
    const QString type = o.value(QLatin1String("type")).toString();
    if (type == QLatin1String("batch")) {
        const QJsonArray items = o.value(QLatin1String("items")).toArray();
        const QJsonDocument arrDoc(items);
        onExtractResult(arrDoc.toJson(QJsonDocument::Compact));
    } else if (type == QLatin1String("error")) {
        finishWithError(o.value(QLatin1String("message")).toString());
    } else if (type == QLatin1String("done")) {
        // Завершение — итог в onProcessFinished
    }
}

void OzonScraper::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    appendStdout(process_->readAllStandardOutput());

    if (!running_)
        return;

    if (status != QProcess::NormalExit || exitCode != 0) {
        QString err = QString::fromUtf8(process_->readAllStandardError()).trimmed();
        if (err.isEmpty())
            err = QStringLiteral("Процесс Python завершился с ошибкой (код %1).").arg(exitCode);
        finishWithError(err);
        return;
    }

    finishWithSuccess();
}

void OzonScraper::onExtractResult(const QByteArray& json)
{
    if (json.isEmpty())
        return;

    const QVector<Product> batch = parseProductsFromJson(json);
    int added = 0;
    for (const Product& p : batch) {
        const QString u = p.url();
        if (u.isEmpty() || seenUrls_.contains(u))
            continue;
        seenUrls_.insert(u);
        allProducts_.append(p);
        added++;
        if (p.price() > 0)
            lastPrice_ = p.price();
    }

    if (added > 0) {
        const int n = allProducts_.size();
        if (n == 1 || n - lastTableCount_ >= UPDATE_TABLE_EVERY_N) {
            lastTableCount_ = n;
            const QVector<Product> top = computeTop50(allProducts_);
            emit statusChanged(QStringLiteral("Найдено товаров"), n, lastPrice_);
            emit topProductsUpdated(top, n);
        }
    }
}

QVector<Product> OzonScraper::parseProductsFromJson(const QByteArray& json)
{
    QVector<Product> out;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return out;
    }

    const QJsonArray arr = doc.array();
    int index = allProducts_.size() + 1;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString url = normalizeUrl(o.value(QLatin1String("url")).toString());
        if (!isValidProductUrl(url))
            continue;
        const QString html = o.value(QLatin1String("html")).toString();
        const std::optional<ParsedTile> parsed = parseOzonTileHtml(html, url);
        if (!parsed.has_value())
            continue;
        const QString name = parsed->name.trimmed();
        if (name.length() < 3)
            continue;
        QString shortName = name;
        if (shortName.length() > 80)
            shortName = shortName.left(77) + QStringLiteral("...");
        out.append(Product(index++, shortName, parsed->price, parsed->reviewPoints, url));
    }
    return out;
}

QVector<Product> OzonScraper::computeTop50(const QVector<Product>& all) const
{
    QVector<Product> filtered;
    for (const Product& p : all) {
        const int pts = p.reviewPoints();
        if (minPoints_ >= 0 && pts < minPoints_)
            continue;
        if (maxPoints_ >= 0 && pts > maxPoints_)
            continue;
        filtered.append(p);
    }
    std::sort(filtered.begin(), filtered.end(), [](const Product& a, const Product& b) {
        return a.pointsToPriceRatio() > b.pointsToPriceRatio();
    });
    const int n = qMin(50, filtered.size());
    QVector<Product> top;
    top.reserve(n);
    for (int i = 0; i < n; ++i) {
        const Product& orig = filtered[i];
        top.append(Product(i + 1, orig.name(), orig.price(), orig.reviewPoints(), orig.url()));
    }
    return top;
}

void OzonScraper::finishWithError(const QString& message)
{
    running_ = false;
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(2000);
    }
    stdoutBuffer_.clear();
    emit finishedWithError(message);
}

void OzonScraper::finishWithSuccess()
{
    running_ = false;

    const QVector<Product> top = computeTop50(allProducts_);
    const int total = allProducts_.size();
    const QString elapsed = formatElapsed(elapsedTimer_.elapsed());

    stdoutBuffer_.clear();

    if (total == 0) {
        emit finishedWithError(QStringLiteral("Товары не найдены. ") + elapsed);
        return;
    }

    emit topProductsUpdated(top, total);
    emit finishedSuccessfully(total, elapsed);
}

QString OzonScraper::formatElapsed(qint64 ms) const
{
    const double sec = ms / 1000.0;
    if (sec < 60)
        return QStringLiteral("Затрачено %1 сек").arg(sec, 0, 'f', 1);
    const int m = static_cast<int>(sec / 60);
    const double s = sec - m * 60;
    if (s < 0.1)
        return QStringLiteral("Затрачено %1 мин").arg(m);
    return QStringLiteral("Затрачено %1 мин %2 сек").arg(m).arg(s, 0, 'f', 1);
}

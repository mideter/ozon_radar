#include "ozonradarscraper.h"
#include "productcardparser.h"

#include <algorithm>
#include <optional>
#include <iostream>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


namespace {


QStringList parseUrlsFromMultiline(const QString& text)
{
    QStringList out;
    const QStringList rawLines = text.split(QChar('\n'));

    for (QString line : rawLines) {
        line = line.trimmed();

        if (line.isEmpty())
            continue;

        if (!line.startsWith("http://", Qt::CaseInsensitive)
            && !line.startsWith("https://", Qt::CaseInsensitive))
            line = QStringLiteral("https://") + line;

        const QUrl u = QUrl::fromUserInput(line);

        if (u.isValid() && !u.scheme().isEmpty())
            out.append(u.toString());
    }
    
    return out;
}


} // namespace


OzonRadarScraper::OzonRadarScraper(QObject* parent)
    : QObject(parent)
    , process_(new QProcess(this))
{
    connect(process_, &QProcess::readyReadStandardOutput, this, &OzonRadarScraper::onProcessStdout);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OzonRadarScraper::onProcessFinished);
}


OzonRadarScraper::~OzonRadarScraper()
{
    stop();
}


QString OzonRadarScraper::resolveFetchScriptPath() const
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


void OzonRadarScraper::start(const QString& urlStr, int minPoints, int maxPoints)
{
    if (running_)
        return;

    fetchScriptPath_ = resolveFetchScriptPath();
    if (!QFileInfo::exists(fetchScriptPath_)) {
        emit finishedWithError(
            QStringLiteral("Не найден скрипт ozon_fetch.py. Укажите OZON_FETCH_SCRIPT или положите "
                           "scripts/ozon_fetch.py рядом с приложением."));
        return;
    }

    const QStringList urls = parseUrlsFromMultiline(urlStr);
    if (urls.isEmpty()) {
        emit finishedWithError(QStringLiteral("Некорректные URL. Укажите по одной ссылке в строке."));
        return;
    }

    allUrls_ = urls;
    urlSessionCount_ = urls.size();
    minPoints_ = minPoints;
    maxPoints_ = maxPoints;
    seenUrls_.clear();
    allProducts_.clear();
    lastTableCount_ = 0;
    lastPrice_ = 0;
    stdoutBuffer_.clear();
    running_ = true;
    elapsedTimer_.start();

    pythonExe_ = qEnvironmentVariable("OZON_PYTHON", QStringLiteral("python3"));

    launchCurrentUrlFetch();
    if (!running_)
        return;
    if (!process_->waitForStarted(5000)) {
        if (process_->state() != QProcess::NotRunning) {
            process_->kill();
            process_->waitForFinished(2000);
        }
        running_ = false;
        allUrls_.clear();
        emit finishedWithError(
            QStringLiteral("Не удалось запустить Python (%1). Установите Python 3 и зависимости "
                           "(см. README).")
                .arg(pythonExe_));
    }
}


void OzonRadarScraper::start(const QUrl& url, int minPoints, int maxPoints)
{
    if (!url.isValid()) {
        emit finishedWithError(QStringLiteral("Некорректный URL."));
        return;
    }
    start(url.toString(), minPoints, maxPoints);
}


void OzonRadarScraper::launchCurrentUrlFetch()
{
    stdoutBuffer_.clear();
    url_ = QUrl(allUrls_.constFirst());
    const int total = allUrls_.size();
    if (total > 1) {
        emit statusChanged(QStringLiteral("0/%1").arg(total), -1, 0);
    } else {
        emit statusChanged(QStringLiteral("Загрузка страницы..."), -1, 0);
    }

    QStringList args;
    args << fetchScriptPath_ << allUrls_;
    process_->start(pythonExe_, args);
}


void OzonRadarScraper::stop()
{
    running_ = false;
    allUrls_.clear();
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(3000);
    }
}


void OzonRadarScraper::onProcessStdout()
{
    appendStdout(process_->readAllStandardOutput());
}


void OzonRadarScraper::appendStdout(const QByteArray& chunk)
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


void OzonRadarScraper::handleJsonLine(const QByteArray& line)
{
    if (!running_)
        return;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonObject o = doc.object();
    const QString type = o.value("type").toString();

    if (type == QStringLiteral("batch")) {
        const QJsonArray items = o.value(QStringLiteral("items")).toArray();
        const QJsonDocument arrDoc(items);
        onExtractResult(arrDoc.toJson(QJsonDocument::Compact));
    } else if (type == QStringLiteral("progress")) {
        const int processed = o.value(QStringLiteral("processed_urls")).toInt();
        const int total = o.value(QStringLiteral("total_urls")).toInt();
        if (total > 1) {
            emit statusChanged(QStringLiteral("%1/%2").arg(processed).arg(total), -1, 0);
        }
    } else if (type == QStringLiteral("error")) {
        finishWithError(o.value(QStringLiteral("message")).toString());
    } else if (type == QStringLiteral("done")) {
        // Завершение — итог в onProcessFinished
    }
}


void OzonRadarScraper::onProcessFinished(int exitCode, QProcess::ExitStatus status)
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

    allUrls_.clear();
    finishWithSuccess();
}


void OzonRadarScraper::onExtractResult(const QByteArray& json)
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
            emit statusChanged(QStringLiteral("Найдено товаров: %1").arg(n), n, lastPrice_);
            emit topProductsUpdated(top, n);
        }
    }
}


QVector<Product> OzonRadarScraper::parseProductsFromJson(const QByteArray& json)
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
        
        const QString url = o.value("url").toString();
        const QString html = o.value("html").toString();
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


QVector<Product> OzonRadarScraper::computeTop50(const QVector<Product>& all) const
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


void OzonRadarScraper::finishWithError(const QString& message)
{
    running_ = false;
    allUrls_.clear();

    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(2000);
    }

    stdoutBuffer_.clear();
    emit finishedWithError(message);
}


void OzonRadarScraper::finishWithSuccess()
{
    running_ = false;

    const QVector<Product> top = computeTop50(allProducts_);
    const int total = allProducts_.size();
    const QString elapsed = formatElapsed(elapsedTimer_.elapsed());

    stdoutBuffer_.clear();

    emit topProductsUpdated(top, total);
    emit finishedSuccessfully(total, elapsed, urlSessionCount_);
}


QString OzonRadarScraper::formatElapsed(qint64 ms) const
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

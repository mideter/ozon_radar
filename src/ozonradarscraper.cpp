#include "ozonradarscraper.h"
#include "fetcheventparser.h"
#include "productcardparser.h"
#include "scraperresultutils.h"

#include <optional>
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
            && !line.startsWith("https://", Qt::CaseInsensitive)) {
            line = "https://" + line;
        }

        const QUrl u = QUrl::fromUserInput(line);

        if (u.isValid() && !u.scheme().isEmpty())
            out.append(u.toString());
    }

    return out;
}


} // namespace


OzonRadarScraper::OzonRadarScraper()
    : process_(new QProcess(this))
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
    QString p = QDir(appDir).filePath("../scripts/ozon_fetch.py");

    if (QFileInfo::exists(p))
        return QDir::cleanPath(p);
    
    p = QDir::currentPath() + "/scripts/ozon_fetch.py";
    
    if (QFileInfo::exists(p))
        return p;
    
    return QDir::cleanPath(QDir(appDir).filePath("../scripts/ozon_fetch.py"));
}


void OzonRadarScraper::start(const QString& urlStr, int minPoints, int maxPoints)
{
    fetchScriptPath_ = resolveFetchScriptPath();

    if (!QFileInfo::exists(fetchScriptPath_)) {
        emit finishedWithError(
            "Не найден скрипт ozon_fetch.py. Укажите OZON_FETCH_SCRIPT или положите "
            "scripts/ozon_fetch.py рядом с приложением.");
        return;
    }

    const QStringList urls = parseUrlsFromMultiline(urlStr);
    if (urls.isEmpty()) {
        emit finishedWithError("Некорректные URL. Укажите по одной ссылке в строке.");
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
    elapsedTimer_.start();

    pythonExe_ = qEnvironmentVariable("OZON_PYTHON", "python3");

    launchCurrentUrlFetch();

    if (!process_->waitForStarted(5000)) {
        if (process_->state() != QProcess::NotRunning) {
            process_->kill();
            process_->waitForFinished(2000);
        }
        allUrls_.clear();
        emit finishedWithError(
            QString("Не удалось запустить Python (%1). Установите Python 3 и зависимости "
                    "(см. README).")
                .arg(pythonExe_));
    }
}


void OzonRadarScraper::launchCurrentUrlFetch()
{
    stdoutBuffer_.clear();
    const int total = allUrls_.size();
    
    if (total > 1)
        emit statusChanged(QString("0/%1").arg(total), -1, 0);
    else
        emit statusChanged("Загрузка страницы...", -1, 0);

    QStringList args;
    args << fetchScriptPath_ << allUrls_;
    process_->start(pythonExe_, args);
}


void OzonRadarScraper::stop()
{
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
    stdoutBuffer_.append(chunk);
    int pos = 0;
    
    while ((pos = stdoutBuffer_.indexOf('\n')) != -1) {
        QByteArray line = stdoutBuffer_.left(pos).trimmed();
        stdoutBuffer_.remove(0, pos + 1);

        if (!line.isEmpty())
            handleJsonLine(line);
    }
}


void OzonRadarScraper::handleJsonLine(const QByteArray& line)
{
    const FetchEvent event = FetchEventParser::parseLine(line);

    switch (event.type) {
    case FetchEvent::Type::Batch:
        onExtractResult(event.batchJson);
        break;
    case FetchEvent::Type::Progress:
        if (event.totalUrls > 1)
            emit statusChanged(QStringLiteral("%1/%2").arg(event.processedUrls).arg(event.totalUrls), -1, 0);
        break;
    case FetchEvent::Type::Error:
        finishWithError(event.errorMessage);
        break;
    case FetchEvent::Type::Done:
        // Завершение — итог в onProcessFinished
        break;
    case FetchEvent::Type::Invalid:
        break;
    }
}


void OzonRadarScraper::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    appendStdout(process_->readAllStandardOutput());

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
    const QVector<Product> batch = parseProductsFromJson(json);
    int added = 0;

    for (const Product& p : batch) {
        const QString u = p.url();

        if (seenUrls_.contains(u))
            continue;
        
        seenUrls_.insert(u);
        allProducts_.append(p);
        added++;

        if (p.price() > 0)
            lastPrice_ = p.price();
    }

    if (added > 0) {
        const int n = allProducts_.size();
        lastTableCount_ = n;
        const QVector<Product> top =
            ScraperResultUtils::computeTopProducts(allProducts_, minPoints_, maxPoints_);
        emit statusChanged(QStringLiteral("Найдено товаров: %1").arg(n), n, lastPrice_);
        emit topProductsUpdated(top, n);
    }
}


QVector<Product> OzonRadarScraper::parseProductsFromJson(const QByteArray& json)
{
    QVector<Product> out;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);

    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return out;

    const QJsonArray arr = doc.array();
    int index = allProducts_.size() + 1;

    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        
        const QString url = o.value("url").toString();
        const QString html = o.value("html").toString();
        const std::optional<ParsedTile> parsed = ParsedTile::parseHtml(html);

        if (!parsed.has_value())
            continue;

        if (parsed->reviewPoints <= 0)
            continue;
        
        QString shortName = parsed->name.trimmed();

        if (shortName.length() > 80)
            shortName = shortName.left(77) + QStringLiteral("...");

        out.append(Product(index++, shortName, parsed->price, parsed->reviewPoints, url));
    }
    
    return out;
}


void OzonRadarScraper::finishWithError(const QString& message)
{
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(2000);
    }

    stdoutBuffer_.clear();
    emit finishedWithError(message);
}


void OzonRadarScraper::finishWithSuccess()
{
    const QVector<Product> top =
        ScraperResultUtils::computeTopProducts(allProducts_, minPoints_, maxPoints_);
    const int total = allProducts_.size();
    const QString elapsed = ScraperResultUtils::formatElapsedText(elapsedTimer_.elapsed());

    stdoutBuffer_.clear();

    emit topProductsUpdated(top, total);
    emit finishedSuccessfully(total, elapsed, urlSessionCount_);
}


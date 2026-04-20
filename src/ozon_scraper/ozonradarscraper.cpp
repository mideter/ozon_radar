#include "ozon_scraper/batchproductmapper.h"
#include "ozon_scraper/ozonradarscraper.h"
#include "ozon_scraper/fetcheventparser.h"
#include "ozon_scraper/scraperresultutils.h"

#include <QCoreApplication>
#include <QDir>


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
    : processRunner_(new PythonFetchProcessRunner(this))
{
    connect(processRunner_, &PythonFetchProcessRunner::stdoutChunk,
            this, &OzonRadarScraper::onProcessStdout);
    connect(processRunner_, &PythonFetchProcessRunner::finished,
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

    const QStringList urls = parseUrlsFromMultiline(urlStr);
    if (urls.isEmpty()) {
        emit finishedWithError("Некорректные URL. Укажите по одной ссылке в строке.");
        return;
    }

    allUrls_ = urls;
    urlSessionCount_ = urls.size();
    minPoints_ = minPoints;
    maxPoints_ = maxPoints;
    productAccumulator_.reset();
    lastTableCount_ = 0;
    stdoutBuffer_.clear();
    elapsedTimer_.start();

    pythonExe_ = qEnvironmentVariable("OZON_PYTHON", "python3");

    launchCurrentUrlFetch();
}


void OzonRadarScraper::launchCurrentUrlFetch()
{
    stdoutBuffer_.clear();
    const int total = allUrls_.size();

    if (total > 1)
        emit statusChanged(QString("0/%1").arg(total), -1, 0);
    else
        emit statusChanged("Загрузка страницы...", -1, 0);

    const PythonFetchStartStatus startStatus =
        processRunner_->startFetch(pythonExe_, fetchScriptPath_, allUrls_);

    if (startStatus == PythonFetchStartStatus::ScriptNotFound) {
        allUrls_.clear();
        emit finishedWithError(
            "Не найден скрипт ozon_fetch.py. Укажите OZON_FETCH_SCRIPT или положите "
            "scripts/ozon_fetch.py рядом с приложением.");
        return;
    }

    if (startStatus == PythonFetchStartStatus::StartFailed) {
        allUrls_.clear();
        emit finishedWithError(
            QString("Не удалось запустить Python (%1). Установите Python 3 и зависимости "
                    "(см. README).")
                .arg(pythonExe_));
    }
}


void OzonRadarScraper::stop()
{
    processRunner_->stop(3000);
}


void OzonRadarScraper::onProcessStdout(const QByteArray& chunk)
{
    appendStdout(chunk);
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


void OzonRadarScraper::onProcessFinished(int exitCode, QProcess::ExitStatus status, const QString& stderrText)
{
    if (status != QProcess::NormalExit || exitCode != 0) {
        QString err = stderrText;
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
    const QVector<Product> batch =
        BatchProductMapper::mapBatchJson(json, productAccumulator_.totalCount() + 1);
    const ProductAccumulatorAddResult addResult = productAccumulator_.addBatch(batch);

    if (addResult.addedCount > 0) {
        const int n = addResult.totalCount;
        lastTableCount_ = n;
        const QVector<Product> top =
            ScraperResultUtils::computeTopProducts(productAccumulator_.allProducts(), minPoints_, maxPoints_);
        emit statusChanged(QStringLiteral("Найдено товаров: %1").arg(n), n, addResult.lastPrice);
        emit topProductsUpdated(top, n);
    }
}


void OzonRadarScraper::finishWithError(const QString& message)
{
    processRunner_->stop(2000);

    stdoutBuffer_.clear();
    emit finishedWithError(message);
}


void OzonRadarScraper::finishWithSuccess()
{
    const QVector<Product> top =
        ScraperResultUtils::computeTopProducts(productAccumulator_.allProducts(), minPoints_, maxPoints_);
    const int total = productAccumulator_.totalCount();
    const QString elapsed = ScraperResultUtils::formatElapsedText(elapsedTimer_.elapsed());

    stdoutBuffer_.clear();

    emit topProductsUpdated(top, total);
    emit finishedSuccessfully(total, elapsed, urlSessionCount_);
}

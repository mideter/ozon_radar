#include "ozon_scraper/batchproductmapper.h"
#include "ozon_scraper/ozonradarscraper.h"
#include "ozon_scraper/scraperresultutils.h"
#include "ozon_scraper/urlinputparser.h"

#include <QCoreApplication>
#include <QDir>


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
    stopRequested_ = false;

    const QStringList urls = UrlInputParser::parseMultiline(urlStr);
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
    fetchEventParser_.reset();
    elapsedTimer_.start();

    pythonExe_ = qEnvironmentVariable("OZON_PYTHON", "python3");

    launchCurrentUrlFetch();
}


void OzonRadarScraper::launchCurrentUrlFetch()
{
    fetchEventParser_.reset();
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
    stopRequested_ = true;
    processRunner_->stop(3000);
}


void OzonRadarScraper::onProcessStdout(const QByteArray& chunk)
{
    const QVector<FetchEvent> events = fetchEventParser_.parseChunk(chunk);
    for (const FetchEvent& event : events)
        handleFetchEvent(event);
}


void OzonRadarScraper::handleFetchEvent(const FetchEvent& event)
{
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
    if (stopRequested_) {
        stopRequested_ = false;
        allUrls_.clear();
        fetchEventParser_.reset();
        emit finishedWithError(QStringLiteral("Загрузка остановлена пользователем."));
        return;
    }

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
    stopRequested_ = false;
    processRunner_->stop(2000);

    fetchEventParser_.reset();
    emit finishedWithError(message);
}


void OzonRadarScraper::finishWithSuccess()
{
    stopRequested_ = false;
    const QVector<Product> top =
        ScraperResultUtils::computeTopProducts(productAccumulator_.allProducts(), minPoints_, maxPoints_);
    const int total = productAccumulator_.totalCount();
    const QString elapsed = ScraperResultUtils::formatElapsedText(elapsedTimer_.elapsed());

    fetchEventParser_.reset();

    emit topProductsUpdated(top, total);
    emit finishedSuccessfully(total, elapsed, urlSessionCount_);
}

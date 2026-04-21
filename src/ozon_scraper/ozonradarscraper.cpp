#include "ozon_scraper/batchproductmapper.h"
#include "ozon_scraper/ozonradarscraper.h"
#include "ozon_scraper/fetcheventparser.h"
#include "ozon_scraper/scraperresultutils.h"

#include <exception>


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


void OzonRadarScraper::start(const QString& urlStr, int minPoints, int maxPoints)
try {
    stopRequested_ = false;

    settings_ = OzonRadarScraperSettings(urlStr, minPoints, maxPoints);

    productAccumulator_.reset();
    fetchEventParser_ = FetchEventParser();
    elapsedTimer_.start();

    launchCurrentUrlFetch();
} 
catch (const std::exception& ex) {
    emit finishedWithError(ex.what());
}


void OzonRadarScraper::launchCurrentUrlFetch()
{
    const QStringList urls = settings_->urls();
    const int total = urls.size();

    if (total > 1)
        emit statusChanged(QString("0/%1").arg(total), -1, 0);
    else
        emit statusChanged("Загрузка страницы...", -1, 0);

    try {
        processRunner_->startFetch(urls);
    } catch (const std::exception& ex) {
        emit finishedWithError(QString::fromUtf8(ex.what()));
    }
}


void OzonRadarScraper::stop()
{
    stopRequested_ = true;
    processRunner_->stop(3000);
}


void OzonRadarScraper::onProcessStdout(const QByteArray& chunk)
{
    const QVector<FetchEvent> events = fetchEventParser_->parseChunk(chunk);
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

    finishWithSuccess();
}


void OzonRadarScraper::onExtractResult(const QByteArray& json)
{
    const QVector<Product> batch =
        BatchProductMapper::mapBatchJson(json, productAccumulator_.totalCount() + 1);
    const ProductAccumulatorAddResult addResult = productAccumulator_.addBatch(batch);

    if (addResult.addedCount > 0) {
        const int n = addResult.totalCount;
        const QVector<Product> top =
            ScraperResultUtils::computeTopProducts(productAccumulator_.allProducts(), settings_->minPoints(), settings_->maxPoints());
        emit statusChanged(QStringLiteral("Найдено товаров: %1").arg(n), n, addResult.lastPrice);
        emit topProductsUpdated(top, n);
    }
}


void OzonRadarScraper::finishWithError(const QString& message)
{
    stopRequested_ = false;
    processRunner_->stop(2000);

    emit finishedWithError(message);
}


void OzonRadarScraper::finishWithSuccess()
{
    stopRequested_ = false;
    const QVector<Product> top =
        ScraperResultUtils::computeTopProducts(productAccumulator_.allProducts(), settings_->minPoints(), settings_->maxPoints());
    const int total = productAccumulator_.totalCount();
    const QString elapsed = ScraperResultUtils::formatElapsedText(elapsedTimer_.elapsed());

    emit topProductsUpdated(top, total);
    emit finishedSuccessfully(total, elapsed, settings_->urls().size());
}

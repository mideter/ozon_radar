#pragma once

#include "ozon_scraper/fetcheventparser.h"
#include "ozon_scraper/pythonfetchprocessrunner.h"
#include "ozon_scraper/productaccumulator.h"
#include "product.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QVector>

class OzonRadarScraper : public QObject
{
    Q_OBJECT

public:
    explicit OzonRadarScraper();
    ~OzonRadarScraper() override;

    Q_INVOKABLE void start(const QString& urlStr, int minPoints, int maxPoints);
    Q_INVOKABLE void stop();

signals:
    void statusChanged(const QString& message, int count, int lastPrice);
    void topProductsUpdated(const QVector<Product>& products, int totalCount);
    void finishedSuccessfully(int totalCount, const QString& elapsedText, int urlCount);
    void finishedWithError(const QString& message);

private slots:
    void onProcessStdout(const QByteArray& chunk);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status, const QString& stderrText);

private:
    QString resolveFetchScriptPath() const;
    void launchCurrentUrlFetch();
    void handleFetchEvent(const FetchEvent& event);
    void onExtractResult(const QByteArray& jsonArrayUtf8);
    void finishWithError(const QString& message);
    void finishWithSuccess();

    PythonFetchProcessRunner* processRunner_ = nullptr;
    FetchEventParser fetchEventParser_;
    QElapsedTimer elapsedTimer_;

    QStringList allUrls_;
    QString fetchScriptPath_;
    QString pythonExe_;
    int minPoints_ = -1;
    int maxPoints_ = -1;
    ProductAccumulator productAccumulator_;
    int lastTableCount_ = 0;
    int urlSessionCount_ = 1;
    bool stopRequested_ = false;
};

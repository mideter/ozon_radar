#pragma once

#include "product.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVector>

class OzonRadarScraper : public QObject
{
    Q_OBJECT

public:
    explicit OzonRadarScraper();
    ~OzonRadarScraper() override;

    Q_INVOKABLE void start(const QString& urlStr, int minPoints, int maxPoints);
    void stop();

signals:
    void statusChanged(const QString& message, int count, int lastPrice);
    void topProductsUpdated(const QVector<Product>& products, int totalCount);
    void finishedSuccessfully(int totalCount, const QString& elapsedText, int urlCount);
    void finishedWithError(const QString& message);

private slots:
    void onProcessStdout();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString resolveFetchScriptPath() const;
    void launchCurrentUrlFetch();
    void appendStdout(const QByteArray& chunk);
    void handleJsonLine(const QByteArray& line);
    void onExtractResult(const QByteArray& jsonArrayUtf8);
    void finishWithError(const QString& message);
    void finishWithSuccess();

    QProcess* process_ = nullptr;
    QByteArray stdoutBuffer_;
    QElapsedTimer elapsedTimer_;

    QStringList allUrls_;
    QString fetchScriptPath_;
    QString pythonExe_;
    int minPoints_ = -1;
    int maxPoints_ = -1;
    QSet<QString> seenUrls_;
    QVector<Product> allProducts_;
    int lastTableCount_ = 0;
    int lastPrice_ = 0;
    int urlSessionCount_ = 1;
};

#include "ozon_scraper/pythonfetchprocessrunner.h"
#include "ozon_scraper/fetchscriptpathresolver.h"

#include <QFileInfo>
#include <stdexcept>


PythonFetchProcessRunner::PythonFetchProcessRunner(QObject* parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &PythonFetchProcessRunner::onReadyReadStandardOutput);
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonFetchProcessRunner::onProcessFinished);
}


void PythonFetchProcessRunner::startFetch(const QStringList& urls)
{
    const QString scriptPath = FetchScriptPathResolver::resolve();
    const QString pythonExe = qEnvironmentVariable("OZON_PYTHON", "python3");

    if (!QFileInfo::exists(scriptPath))
        throw std::runtime_error(
            "Не найден скрипт ozon_fetch.py. Укажите OZON_FETCH_SCRIPT или положите scripts/ozon_fetch.py рядом с приложением.");

    if (isRunning())
        stop();

    QStringList args;
    args << scriptPath << urls;

    process_.start(pythonExe, args);
    if (process_.waitForStarted(5000))
        return;

    if (isRunning())
        stop(2000);

    throw std::runtime_error(
        QString("Не удалось запустить Python (%1). Установите Python 3 и зависимости (см. README).")
            .arg(pythonExe)
            .toStdString());
}


void PythonFetchProcessRunner::stop(int waitMs)
{
    if (!isRunning())
        return;

    process_.terminate();
    if (process_.waitForFinished(waitMs))
        return;

    process_.kill();
    process_.waitForFinished(1000);
}


bool PythonFetchProcessRunner::isRunning() const
{
    return process_.state() != QProcess::NotRunning;
}


void PythonFetchProcessRunner::onReadyReadStandardOutput()
{
    const QByteArray chunk = process_.readAllStandardOutput();
    if (!chunk.isEmpty())
        emit stdoutChunk(chunk);
}


void PythonFetchProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    const QByteArray trailingStdout = process_.readAllStandardOutput();
    if (!trailingStdout.isEmpty())
        emit stdoutChunk(trailingStdout);

    const QString stderrText = QString::fromUtf8(process_.readAllStandardError()).trimmed();
    emit finished(exitCode, status, stderrText);
}

#include "ozon_scraper/pythonfetchprocessrunner.h"

#include <QFileInfo>


PythonFetchProcessRunner::PythonFetchProcessRunner(QObject* parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &PythonFetchProcessRunner::onReadyReadStandardOutput);
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonFetchProcessRunner::onProcessFinished);
}


PythonFetchStartStatus PythonFetchProcessRunner::startFetch(const QString& pythonExe,
                                                            const QString& scriptPath,
                                                            const QStringList& urls)
{
    if (!QFileInfo::exists(scriptPath))
        return PythonFetchStartStatus::ScriptNotFound;

    if (isRunning())
        stop();

    QStringList args;
    args << scriptPath << urls;

    process_.start(pythonExe, args);
    if (process_.waitForStarted(5000))
        return PythonFetchStartStatus::Started;

    if (isRunning())
        stop(2000);

    return PythonFetchStartStatus::StartFailed;
}


void PythonFetchProcessRunner::stop(int waitMs)
{
    if (!isRunning())
        return;

    process_.kill();
    process_.waitForFinished(waitMs);
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

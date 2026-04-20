#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

enum class PythonFetchStartStatus {
    Started,
    ScriptNotFound,
    StartFailed
};

class PythonFetchProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit PythonFetchProcessRunner(QObject* parent = nullptr);

    PythonFetchStartStatus startFetch(const QString& pythonExe,
                                      const QString& scriptPath,
                                      const QStringList& urls);
    void stop(int waitMs = 3000);
    bool isRunning() const;

signals:
    void stdoutChunk(const QByteArray& chunk);
    void finished(int exitCode, QProcess::ExitStatus status, const QString& stderrText);

private slots:
    void onReadyReadStandardOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess process_;
};

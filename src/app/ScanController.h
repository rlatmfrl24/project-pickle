#pragma once

#include "application/ScanCommitResult.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <memory>

class CancellationToken;

class ScanController : public QObject
{
    Q_OBJECT

public:
    explicit ScanController(QObject *parent = nullptr);
    ~ScanController() override;

    bool isRunning() const;
    bool cancelAvailable() const;
    void startScan(const QString &rootPath, const QString &databasePath);
    bool cancel();
    void clearCurrentRoot();
    void waitForFinished();

signals:
    void scanRejected(const QString &status, const QString &currentScanRoot);
    void scanStarted(const QString &status, const QString &currentScanRoot);
    void progressChanged(int visitedCount, int foundCount, const QString &progressText);
    void scanFinished(const ScanCommitResult &result);

private:
    void handleFinished();

    QFutureWatcher<ScanCommitResult> m_watcher;
    std::shared_ptr<CancellationToken> m_cancellation;
    QString m_currentRoot;
};

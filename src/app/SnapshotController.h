#pragma once

#include "domain/MediaEntities.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <memory>

class CancellationToken;

class SnapshotController : public QObject
{
    Q_OBJECT

public:
    explicit SnapshotController(QObject *parent = nullptr);
    ~SnapshotController() override;

    bool isRunning() const;
    bool cancelAvailable() const;
    void startCapture(const SnapshotCaptureRequest &request, const QString &displayName);
    bool cancel();
    void waitForFinished();

signals:
    void captureRejected(const QString &status);
    void captureStarted(const QString &status);
    void captureFinished(const SnapshotCaptureResult &result);

private:
    void handleFinished();

    QFutureWatcher<SnapshotCaptureResult> m_watcher;
    std::shared_ptr<CancellationToken> m_cancellation;
};

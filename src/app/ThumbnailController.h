#pragma once

#include "domain/MediaEntities.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

class CancellationToken;

struct ThumbnailMaintenanceResult
{
    QVector<ThumbnailGenerationResult> generated;
    bool canceled = false;
    int processedCount = 0;
    int totalCount = 0;
};

class ThumbnailController : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailController(QObject *parent = nullptr);
    ~ThumbnailController() override;

    bool isRunning() const;
    bool cancelAvailable() const;
    void startRebuild(
        const QVector<ThumbnailBackfillItem> &items,
        const QString &thumbnailRoot,
        int maxWidth = 320,
        int quality = 82);
    bool cancel();
    void waitForFinished();

signals:
    void rebuildRejected(const QString &status);
    void rebuildStarted(const QString &status);
    void rebuildProgressed(const QString &status);
    void rebuildFinished(const ThumbnailMaintenanceResult &result);

private:
    void handleFinished();

    QFutureWatcher<ThumbnailMaintenanceResult> m_watcher;
    std::shared_ptr<CancellationToken> m_cancellation;
};

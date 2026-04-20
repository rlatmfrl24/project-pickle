#include "app/ThumbnailController.h"

#include "domain/CancellationToken.h"
#include "media/ThumbnailService.h"

#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

ThumbnailController::ThumbnailController(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<ThumbnailMaintenanceResult>::finished, this, &ThumbnailController::handleFinished);
}

ThumbnailController::~ThumbnailController()
{
    disconnect(&m_watcher, nullptr, this, nullptr);
    if (m_cancellation) {
        m_cancellation->cancel();
    }
    waitForFinished();
}

bool ThumbnailController::isRunning() const
{
    return m_watcher.isRunning();
}

bool ThumbnailController::cancelAvailable() const
{
    return m_watcher.isRunning()
        && m_cancellation
        && !m_cancellation->isCancellationRequested();
}

void ThumbnailController::startRebuild(
    const QVector<ThumbnailBackfillItem> &items,
    const QString &thumbnailRoot,
    int maxWidth,
    int quality)
{
    if (m_watcher.isRunning()) {
        emit rebuildRejected(QStringLiteral("Thumbnail rebuild already in progress"));
        return;
    }

    if (items.isEmpty()) {
        emit rebuildRejected(QStringLiteral("No thumbnails to rebuild."));
        return;
    }

    m_cancellation = std::make_shared<CancellationToken>();
    const std::shared_ptr<CancellationToken> cancellation = m_cancellation;
    const QPointer<ThumbnailController> self(this);
    const int effectiveMaxWidth = maxWidth > 0 ? maxWidth : 320;
    const int effectiveQuality = quality >= 0 && quality <= 100 ? quality : 82;
    emit rebuildStarted(QStringLiteral("Rebuilding thumbnail cache: 0/%1").arg(items.size()));

    m_watcher.setFuture(QtConcurrent::run([items, thumbnailRoot, effectiveMaxWidth, effectiveQuality, cancellation, self]() {
        ThumbnailMaintenanceResult maintenance;
        maintenance.totalCount = items.size();

        ThumbnailService thumbnailService;
        for (const ThumbnailBackfillItem &item : items) {
            if (cancellation && cancellation->isCancellationRequested()) {
                break;
            }

            ThumbnailGenerationResult result;
            result.mediaId = item.mediaId;
            if (ThumbnailService::isUsableThumbnail(item.existingThumbnailPath, effectiveMaxWidth)) {
                result.thumbnailPath = item.existingThumbnailPath;
                result.skipped = true;
                result.succeeded = true;
            } else {
                ThumbnailGenerationRequest request;
                request.mediaId = item.mediaId;
                request.sourceImagePath = item.sourceImagePath;
                request.outputRootPath = thumbnailRoot;
                request.maxWidth = effectiveMaxWidth;
                request.quality = effectiveQuality;
                result = thumbnailService.generate(request, cancellation);
            }

            if (result.succeeded && !result.skipped) {
                maintenance.generated.append(result);
            }

            ++maintenance.processedCount;
            if (self) {
                const int processedCount = maintenance.processedCount;
                const int totalCount = maintenance.totalCount;
                QMetaObject::invokeMethod(self.data(), [self, processedCount, totalCount]() {
                    if (self && self->isRunning()) {
                        emit self->rebuildProgressed(
                            QStringLiteral("Rebuilding thumbnail cache: %1/%2").arg(processedCount).arg(totalCount));
                    }
                }, Qt::QueuedConnection);
            }
        }

        maintenance.canceled = cancellation && cancellation->isCancellationRequested();
        return maintenance;
    }));
}

bool ThumbnailController::cancel()
{
    if (!cancelAvailable()) {
        return false;
    }

    m_cancellation->cancel();
    return true;
}

void ThumbnailController::waitForFinished()
{
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void ThumbnailController::handleFinished()
{
    const ThumbnailMaintenanceResult result = m_watcher.result();
    m_cancellation.reset();
    emit rebuildFinished(result);
}

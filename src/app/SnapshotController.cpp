#include "app/SnapshotController.h"

#include "application/CaptureSnapshotUseCase.h"
#include "domain/CancellationToken.h"
#include "media/SnapshotService.h"

#include <QtConcurrent>

SnapshotController::SnapshotController(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<SnapshotCaptureResult>::finished, this, &SnapshotController::handleFinished);
}

SnapshotController::~SnapshotController()
{
    disconnect(&m_watcher, nullptr, this, nullptr);
    if (m_cancellation) {
        m_cancellation->cancel();
    }
    waitForFinished();
}

bool SnapshotController::isRunning() const
{
    return m_watcher.isRunning();
}

bool SnapshotController::cancelAvailable() const
{
    return m_watcher.isRunning()
        && m_cancellation
        && !m_cancellation->isCancellationRequested();
}

void SnapshotController::startCapture(const SnapshotCaptureRequest &request, const QString &displayName)
{
    if (request.mediaId <= 0 || request.filePath.isEmpty()) {
        emit captureRejected(QStringLiteral("Select a media item before capturing a snapshot."));
        return;
    }

    if (m_watcher.isRunning()) {
        emit captureRejected(QStringLiteral("Snapshot capture already in progress"));
        return;
    }

    m_cancellation = std::make_shared<CancellationToken>();
    const std::shared_ptr<CancellationToken> cancellation = m_cancellation;
    const QString statusName = displayName.isEmpty() ? request.filePath : displayName;
    emit captureStarted(QStringLiteral("Capturing snapshot: %1").arg(statusName));

    m_watcher.setFuture(QtConcurrent::run([request, cancellation]() {
        SnapshotService snapshotService;
        CaptureSnapshotUseCase useCase(&snapshotService);
        return useCase.execute(request, cancellation);
    }));
}

bool SnapshotController::cancel()
{
    if (!cancelAvailable()) {
        return false;
    }

    m_cancellation->cancel();
    return true;
}

void SnapshotController::waitForFinished()
{
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void SnapshotController::handleFinished()
{
    const SnapshotCaptureResult result = m_watcher.result();
    m_cancellation.reset();
    emit captureFinished(result);
}

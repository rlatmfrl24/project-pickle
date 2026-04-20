#include "app/MetadataController.h"

#include "application/ExtractMetadataUseCase.h"
#include "domain/CancellationToken.h"
#include "media/MetadataService.h"

#include <QtConcurrent>

MetadataController::MetadataController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MetadataControllerResult>("MetadataControllerResult");
    connect(&m_watcher, &QFutureWatcher<MetadataControllerResult>::finished, this, &MetadataController::handleFinished);
}

MetadataController::~MetadataController()
{
    disconnect(&m_watcher, nullptr, this, nullptr);
    if (m_cancellation) {
        m_cancellation->cancel();
    }
    waitForFinished();
}

bool MetadataController::isRunning() const
{
    return m_watcher.isRunning();
}

bool MetadataController::cancelAvailable() const
{
    return m_watcher.isRunning()
        && m_cancellation
        && !m_cancellation->isCancellationRequested();
}

void MetadataController::startExtraction(
    int mediaId,
    const QString &filePath,
    const QString &displayName,
    bool manual,
    const QString &ffprobeProgram)
{
    if (mediaId <= 0 || filePath.isEmpty()) {
        emit extractionRejected(QStringLiteral("Select a media item before refreshing metadata."));
        return;
    }

    if (m_watcher.isRunning()) {
        emit extractionRejected(QStringLiteral("Metadata refresh already in progress"));
        return;
    }

    m_cancellation = std::make_shared<CancellationToken>();
    const std::shared_ptr<CancellationToken> cancellation = m_cancellation;
    const QString statusName = displayName.isEmpty() ? filePath : displayName;
    emit extractionStarted(QStringLiteral("Reading metadata: %1").arg(statusName));

    m_watcher.setFuture(QtConcurrent::run([mediaId, filePath, ffprobeProgram, manual, cancellation]() {
        MetadataControllerResult result;
        result.mediaId = mediaId;
        result.manual = manual;

        MetadataService metadataService;
        ExtractMetadataUseCase useCase(&metadataService);
        result.extraction = useCase.execute(filePath, ffprobeProgram, cancellation);
        return result;
    }));
}

bool MetadataController::cancel()
{
    if (!cancelAvailable()) {
        return false;
    }

    m_cancellation->cancel();
    return true;
}

void MetadataController::waitForFinished()
{
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void MetadataController::handleFinished()
{
    const MetadataControllerResult result = m_watcher.result();
    m_cancellation.reset();
    emit extractionFinished(result);
}

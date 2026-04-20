#include "app/MediaActionsController.h"

#include "application/RenameMediaUseCase.h"
#include "application/SavePlaybackPositionUseCase.h"
#include "application/UpdateMediaDetailsUseCase.h"
#include "application/UpdateMediaFlagsUseCase.h"
#include "media/MediaLibraryModel.h"
#include "media/RenameService.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"
#include "ports/IMediaRepository.h"

#include <QDateTime>
#include <QFileInfo>

#include <algorithm>

namespace {
QStringList stringListFromVariantList(const QVariantList &values)
{
    QStringList result;
    for (const QVariant &value : values) {
        result.append(value.toString());
    }
    return result;
}

QVariantMap snapshotToMap(const SnapshotItem &snapshot)
{
    return {
        {QStringLiteral("id"), snapshot.id},
        {QStringLiteral("mediaId"), snapshot.mediaId},
        {QStringLiteral("imagePath"), snapshot.imagePath},
        {QStringLiteral("timestampMs"), snapshot.timestampMs},
        {QStringLiteral("createdAt"), snapshot.createdAt},
    };
}
}

MediaActionsController::MediaActionsController(IMediaRepository *repository, MediaLibraryModel *model)
    : m_repository(repository)
    , m_model(model)
{
}

void MediaActionsController::setRepository(IMediaRepository *repository)
{
    m_repository = repository;
}

void MediaActionsController::setModel(MediaLibraryModel *model)
{
    m_model = model;
}

MediaActionResult MediaActionsController::saveDetails(
    const QVariantMap &media,
    const QString &description,
    const QString &reviewStatus,
    int rating,
    const QVariantList &tags,
    bool databaseReady,
    bool hasActiveWork)
{
    MediaActionResult action;
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    if (mediaId <= 0) {
        action.status = QStringLiteral("Select a media item before saving details.");
        return action;
    }
    if (!databaseReady || !m_repository) {
        action.status = QStringLiteral("Database is not ready for saving details.");
        return action;
    }
    if (hasActiveWork) {
        action.status = QStringLiteral("Wait for active work to finish before saving details.");
        return action;
    }

    const QStringList tagNames = stringListFromVariantList(tags);
    UpdateMediaDetailsUseCase useCase(m_repository);
    const VoidResult result = useCase.execute(mediaId, description, reviewStatus, rating, tagNames);
    if (!result.succeeded) {
        action.status = QStringLiteral("Detail save failed: %1").arg(result.errorMessage);
        return action;
    }

    if (m_model) {
        const int row = m_model->indexOfId(mediaId);
        if (row >= 0) {
            MediaLibraryItem item = m_model->itemAt(row);
            item.description = description;
            item.reviewStatus = reviewStatus;
            item.rating = rating;
            item.tags = tagNames;
            action.selectedMediaChanged = m_model->replaceItem(mediaId, item);
        }
    }
    action.libraryReloadRequested = true;
    action.status = QStringLiteral("Details saved");
    return action;
}

MediaActionResult MediaActionsController::renameMedia(
    const QVariantMap &media,
    const QString &newBaseName,
    bool databaseReady,
    bool hasActiveWork)
{
    MediaActionResult action;
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    if (mediaId <= 0 || filePath.isEmpty()) {
        action.status = QStringLiteral("Select a media item before renaming.");
        return action;
    }
    if (!databaseReady || !m_repository) {
        action.status = QStringLiteral("Database is not ready for rename.");
        return action;
    }
    if (hasActiveWork) {
        action.status = QStringLiteral("Wait for active work to finish before renaming.");
        return action;
    }

    const QFileInfo originalInfo(filePath);
    RenameService renameService;
    RenameMediaUseCase useCase(&renameService, m_repository);
    const OperationResult<ScannedMediaFile> renameResult = useCase.execute(
        mediaId,
        filePath,
        newBaseName,
        originalInfo.completeBaseName());
    if (!renameResult.succeeded) {
        action.status = QStringLiteral("Rename failed: %1").arg(renameResult.errorMessage);
        return action;
    }

    if (m_model) {
        const int row = m_model->indexOfId(mediaId);
        if (row >= 0) {
            MediaLibraryItem item = m_model->itemAt(row);
            item.fileName = renameResult.value.fileName;
            item.filePath = renameResult.value.filePath;
            item.fileSizeBytes = renameResult.value.fileSize;
            item.modifiedAt = renameResult.value.modifiedAt;
            action.selectedMediaChanged = m_model->replaceItem(mediaId, item);
        }
    }
    action.libraryReloadRequested = true;
    action.status = QStringLiteral("Renamed to %1").arg(renameResult.value.fileName);
    return action;
}

MediaActionResult MediaActionsController::setFavorite(
    const QVariantMap &media,
    bool enabled,
    bool databaseReady,
    bool hasActiveWork)
{
    MediaActionResult action;
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    if (mediaId <= 0) {
        action.status = QStringLiteral("Select a media item before changing favorite.");
        return action;
    }
    if (!databaseReady || !m_repository || !m_model) {
        action.status = QStringLiteral("Database is not ready for favorite updates.");
        return action;
    }
    if (hasActiveWork) {
        action.status = QStringLiteral("Wait for active work to finish before updating favorite.");
        return action;
    }

    UpdateMediaFlagsUseCase useCase(m_repository);
    const VoidResult result = useCase.execute(mediaId, MediaFlagKind::Favorite, enabled);
    if (!result.succeeded) {
        action.status = QStringLiteral("Favorite update failed: %1").arg(result.errorMessage);
        return action;
    }
    m_model->setFavorite(mediaId, enabled);
    action.selectedMediaChanged = true;
    action.status = enabled ? QStringLiteral("Marked as favorite") : QStringLiteral("Favorite removed");
    return action;
}

MediaActionResult MediaActionsController::setDeleteCandidate(
    const QVariantMap &media,
    bool enabled,
    bool databaseReady,
    bool hasActiveWork)
{
    MediaActionResult action;
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    if (mediaId <= 0) {
        action.status = QStringLiteral("Select a media item before changing delete candidate.");
        return action;
    }
    if (!databaseReady || !m_repository || !m_model) {
        action.status = QStringLiteral("Database is not ready for delete candidate updates.");
        return action;
    }
    if (hasActiveWork) {
        action.status = QStringLiteral("Wait for active work to finish before updating delete candidate.");
        return action;
    }

    UpdateMediaFlagsUseCase useCase(m_repository);
    const VoidResult result = useCase.execute(mediaId, MediaFlagKind::DeleteCandidate, enabled);
    if (!result.succeeded) {
        action.status = QStringLiteral("Delete candidate update failed: %1").arg(result.errorMessage);
        return action;
    }
    m_model->setDeleteCandidate(mediaId, enabled);
    action.selectedMediaChanged = true;
    action.status = enabled ? QStringLiteral("Marked as delete candidate") : QStringLiteral("Delete candidate removed");
    return action;
}

MediaActionResult MediaActionsController::savePlaybackPosition(
    const QVariantMap &media,
    qint64 positionMs,
    bool databaseReady)
{
    MediaActionResult action;
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    if (mediaId <= 0 || !databaseReady || !m_repository || !m_model) {
        return action;
    }

    const qint64 normalizedPositionMs = std::max<qint64>(0, positionMs);
    const qint64 currentPositionMs = media.value(QStringLiteral("lastPositionMs")).toLongLong();
    const qint64 positionDelta = currentPositionMs > normalizedPositionMs
        ? currentPositionMs - normalizedPositionMs
        : normalizedPositionMs - currentPositionMs;
    if (positionDelta < 1000) {
        return action;
    }

    const QString playedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    SavePlaybackPositionUseCase useCase(m_repository);
    const VoidResult result = useCase.execute(mediaId, normalizedPositionMs, playedAt);
    if (!result.succeeded) {
        action.status = QStringLiteral("Playback position save failed: %1").arg(result.errorMessage);
        return action;
    }
    m_model->setPlaybackPosition(mediaId, normalizedPositionMs, playedAt);
    action.selectedMediaChanged = true;
    return action;
}

MediaActionResult MediaActionsController::refreshMediaFromRepository(int mediaId)
{
    MediaActionResult action;
    if (mediaId <= 0 || !m_repository || !m_model) {
        return action;
    }

    const MediaLibraryItem item = m_repository->fetchMediaById(mediaId);
    if (item.id <= 0) {
        const QString error = m_repository->lastError();
        if (!error.isEmpty()) {
            action.status = QStringLiteral("Media refresh failed: %1").arg(error);
        }
        return action;
    }

    action.selectedMediaChanged = m_model->replaceItem(mediaId, item);
    return action;
}

MediaActionResult MediaActionsController::applySnapshotResult(const SnapshotCaptureResult &result)
{
    MediaActionResult action;
    if (result.canceled) {
        action.status = QStringLiteral("Snapshot canceled");
        return action;
    }
    if (!result.succeeded) {
        const QString errorMessage = result.errorMessage.isEmpty()
            ? QStringLiteral("Snapshot capture did not return a result.")
            : result.errorMessage;
        action.status = QStringLiteral("Snapshot failed: %1").arg(errorMessage);
        return action;
    }
    if (!m_repository || !m_repository->addSnapshot(result.mediaId, result.imagePath, result.timestampMs)) {
        action.status = QStringLiteral("Snapshot DB insert failed: %1").arg(m_repository ? m_repository->lastError() : QStringLiteral("repository unavailable"));
        return action;
    }

    const QString thumbnailPath = result.thumbnailPath.isEmpty() ? result.imagePath : result.thumbnailPath;
    if (!m_repository->setMediaThumbnailPath(result.mediaId, thumbnailPath)) {
        action.status = QStringLiteral("Thumbnail update failed: %1").arg(m_repository->lastError());
        return action;
    }
    if (m_model) {
        m_model->setThumbnailPath(result.mediaId, thumbnailPath);
    }
    action.selectedMediaChanged = true;
    action.status = QStringLiteral("Snapshot captured");
    return action;
}

MediaActionResult MediaActionsController::applyThumbnailMaintenanceResult(
    const QVector<ThumbnailGenerationResult> &results,
    bool canceled)
{
    MediaActionResult action;
    int updatedCount = 0;
    for (const ThumbnailGenerationResult &generated : results) {
        if (!generated.succeeded || generated.thumbnailPath.isEmpty()) {
            continue;
        }
        if (m_repository && m_repository->setMediaThumbnailPath(generated.mediaId, generated.thumbnailPath)) {
            ++updatedCount;
            if (m_model) {
                m_model->setThumbnailPath(generated.mediaId, generated.thumbnailPath);
            }
        }
    }
    action.selectedMediaChanged = updatedCount > 0;
    action.status = canceled
        ? QStringLiteral("Thumbnail rebuild canceled: %1 updated").arg(updatedCount)
        : QStringLiteral("Thumbnail rebuild complete: %1 updated").arg(updatedCount);
    return action;
}

LibraryResetResult MediaActionsController::resetLibrary()
{
    LibraryResetResult result;
    if (!m_repository || !m_model) {
        result.libraryStatus = QStringLiteral("Database is not ready for library reset.");
        return result;
    }
    if (!m_repository->resetLibraryData()) {
        result.libraryStatus = QStringLiteral("Library reset failed: %1").arg(m_repository->lastError());
        return result;
    }

    QString snapshotClearError;
    const bool snapshotsCleared = SnapshotService::clearSnapshotRoot(SnapshotService::defaultSnapshotRoot(), &snapshotClearError);
    QString thumbnailClearError;
    const bool thumbnailsCleared = ThumbnailService::clearThumbnailRoot(ThumbnailService::defaultThumbnailRoot(), &thumbnailClearError);

    m_model->setItems({});
    result.succeeded = true;
    result.selectedMediaChanged = true;
    result.snapshotStatus = snapshotsCleared ? QString() : QStringLiteral("Snapshot cleanup failed: %1").arg(snapshotClearError);
    result.thumbnailStatus = thumbnailsCleared ? QString() : QStringLiteral("Thumbnail cleanup failed: %1").arg(thumbnailClearError);
    result.fileActionStatus = QStringLiteral("Library reset complete");
    result.libraryStatus = QStringLiteral("Library reset complete: 0 item(s)");
    return result;
}

QVariantList MediaActionsController::snapshotsForMedia(int mediaId, bool databaseReady) const
{
    QVariantList snapshots;
    if (!databaseReady || !m_repository || mediaId <= 0) {
        return snapshots;
    }

    const QVector<SnapshotItem> items = m_repository->fetchSnapshotsForMedia(mediaId);
    if (!m_repository->lastError().isEmpty()) {
        return {};
    }
    for (const SnapshotItem &snapshot : items) {
        snapshots.append(snapshotToMap(snapshot));
    }
    return snapshots;
}

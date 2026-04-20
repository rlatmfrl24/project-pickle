#pragma once

#include "domain/MediaEntities.h"

#include <QVariantList>
#include <QVariantMap>

class IMediaRepository;
class MediaLibraryModel;

struct MediaActionResult
{
    QString status;
    bool selectedMediaChanged = false;
    bool libraryReloadRequested = false;
};

struct LibraryResetResult
{
    QString libraryStatus;
    QString fileActionStatus;
    QString snapshotStatus;
    QString thumbnailStatus;
    bool succeeded = false;
    bool selectedMediaChanged = false;
};

class MediaActionsController
{
public:
    MediaActionsController(IMediaRepository *repository = nullptr, MediaLibraryModel *model = nullptr);

    void setRepository(IMediaRepository *repository);
    void setModel(MediaLibraryModel *model);

    MediaActionResult saveDetails(
        const QVariantMap &media,
        const QString &description,
        const QString &reviewStatus,
        int rating,
        const QVariantList &tags,
        bool databaseReady,
        bool hasActiveWork);
    MediaActionResult renameMedia(
        const QVariantMap &media,
        const QString &newBaseName,
        bool databaseReady,
        bool hasActiveWork);
    MediaActionResult setFavorite(const QVariantMap &media, bool enabled, bool databaseReady, bool hasActiveWork);
    MediaActionResult setDeleteCandidate(const QVariantMap &media, bool enabled, bool databaseReady, bool hasActiveWork);
    MediaActionResult savePlaybackPosition(const QVariantMap &media, qint64 positionMs, bool databaseReady);
    MediaActionResult applySnapshotResult(const SnapshotCaptureResult &result);
    MediaActionResult applyThumbnailMaintenanceResult(const QVector<ThumbnailGenerationResult> &results, bool canceled);
    LibraryResetResult resetLibrary();
    QVariantList snapshotsForMedia(int mediaId, bool databaseReady) const;

private:
    IMediaRepository *m_repository = nullptr;
    MediaLibraryModel *m_model = nullptr;
};

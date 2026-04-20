#pragma once

#include "domain/LibraryQuery.h"
#include "domain/MediaEntities.h"

#include <QString>
#include <QStringList>
#include <QVector>

class IMediaRepository
{
public:
    virtual ~IMediaRepository() = default;

    virtual bool upsertScanResult(
        const QString &rootPath,
        const QVector<ScannedMediaFile> &files,
        MediaUpsertResult *result = nullptr) = 0;
    virtual QVector<MediaLibraryItem> fetchLibraryItems() = 0;
    virtual QVector<MediaLibraryItem> fetchLibraryItems(const MediaLibraryQuery &query) = 0;
    virtual MediaLibraryItem fetchMediaById(int mediaId) = 0;
    virtual bool renameMediaFile(int mediaId, const ScannedMediaFile &file) = 0;
    virtual bool setMediaFavorite(int mediaId, bool enabled) = 0;
    virtual bool setMediaDeleteCandidate(int mediaId, bool enabled) = 0;
    virtual bool updatePlaybackPosition(int mediaId, qint64 positionMs, const QString &playedAt) = 0;
    virtual bool resetLibraryData() = 0;
    virtual bool updateMediaMetadata(int mediaId, const MediaMetadata &metadata) = 0;
    virtual bool updateMediaDetails(
        int mediaId,
        const QString &description,
        const QString &reviewStatus,
        int rating,
        const QStringList &tags) = 0;
    virtual bool addSnapshot(int mediaId, const QString &imagePath, qint64 timestampMs, int *snapshotId = nullptr) = 0;
    virtual QVector<SnapshotItem> fetchSnapshotsForMedia(int mediaId) = 0;
    virtual bool setMediaThumbnailPath(int mediaId, const QString &imagePath) = 0;
    virtual QVector<ThumbnailBackfillItem> fetchThumbnailBackfillItems() = 0;
    virtual QString lastError() const = 0;
};

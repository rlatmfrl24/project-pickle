#pragma once

#include "media/MediaTypes.h"

#include <QSqlDatabase>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class MediaRepository
{
public:
    explicit MediaRepository(QSqlDatabase database);

    bool upsertScanResult(
        const QString &rootPath,
        const QVector<ScannedMediaFile> &files,
        MediaUpsertResult *result = nullptr);

    QVector<MediaLibraryItem> fetchLibraryItems();
    QVector<MediaLibraryItem> fetchLibraryItems(const MediaLibraryQuery &query);
    bool renameMediaFile(int mediaId, const ScannedMediaFile &file);
    bool setMediaFavorite(int mediaId, bool enabled);
    bool setMediaDeleteCandidate(int mediaId, bool enabled);
    bool updatePlaybackPosition(int mediaId, qint64 positionMs, const QString &playedAt);
    bool resetLibraryData();
    bool updateMediaMetadata(int mediaId, const MediaMetadata &metadata);
    bool updateMediaDetails(
        int mediaId,
        const QString &description,
        const QString &reviewStatus,
        int rating,
        const QStringList &tags);
    bool addSnapshot(int mediaId, const QString &imagePath, qint64 timestampMs, int *snapshotId = nullptr);
    QVector<SnapshotItem> fetchSnapshotsForMedia(int mediaId);
    bool setMediaThumbnailPath(int mediaId, const QString &imagePath);
    QVector<ThumbnailBackfillItem> fetchThumbnailBackfillItems();
    QString lastError() const;

private:
    bool upsertScanRoot(const QString &rootPath);
    bool upsertMediaFile(const ScannedMediaFile &file);
    QHash<int, QStringList> tagsByMediaId(bool *ok);
    int tagIdForName(const QString &tagName);
    bool ensureOpen();
    void setLastError(const QString &error);

    QSqlDatabase m_database;
    QString m_lastError;
};

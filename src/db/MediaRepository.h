#pragma once

#include "media/MediaTypes.h"
#include "ports/IMediaRepository.h"

#include <QSqlDatabase>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class QSqlQuery;

class MediaRepository : public IMediaRepository
{
public:
    explicit MediaRepository(QSqlDatabase database);

    bool upsertScanResult(
        const QString &rootPath,
        const QVector<ScannedMediaFile> &files,
        MediaUpsertResult *result = nullptr) override;

    QVector<MediaLibraryItem> fetchLibraryItems() override;
    QVector<MediaLibraryItem> fetchLibraryItems(const MediaLibraryQuery &query) override;
    MediaLibraryItem fetchMediaById(int mediaId) override;
    bool renameMediaFile(int mediaId, const ScannedMediaFile &file) override;
    bool setMediaFavorite(int mediaId, bool enabled) override;
    bool setMediaDeleteCandidate(int mediaId, bool enabled) override;
    bool updatePlaybackPosition(int mediaId, qint64 positionMs, const QString &playedAt) override;
    bool resetLibraryData() override;
    bool updateMediaMetadata(int mediaId, const MediaMetadata &metadata) override;
    bool updateMediaDetails(
        int mediaId,
        const QString &description,
        const QString &reviewStatus,
        int rating,
        const QStringList &tags) override;
    bool addSnapshot(int mediaId, const QString &imagePath, qint64 timestampMs, int *snapshotId = nullptr) override;
    QVector<SnapshotItem> fetchSnapshotsForMedia(int mediaId) override;
    bool setMediaThumbnailPath(int mediaId, const QString &imagePath) override;
    QVector<ThumbnailBackfillItem> fetchThumbnailBackfillItems() override;
    QString lastError() const override;

private:
    bool upsertScanRoot(const QString &rootPath);
    bool prepareMediaFileUpsert(QSqlQuery *query);
    bool upsertMediaFile(QSqlQuery *query, const ScannedMediaFile &file);
    QHash<int, QStringList> tagsByMediaIds(const QVector<int> &mediaIds, bool *ok);
    int tagIdForName(const QString &tagName);
    bool ensureOpen();
    void setLastError(const QString &error);

    QSqlDatabase m_database;
    QString m_lastError;
};

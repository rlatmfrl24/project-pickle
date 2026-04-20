#include "MediaRepository.h"

#include "core/PathSecurity.h"

#include <QHash>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QVariant>

#include <cmath>
#include <utility>

namespace {
QString escapedLikePattern(QString text)
{
    text.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    text.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    text.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return QStringLiteral("%%1%").arg(text);
}

QString defaultThumbnailRoot()
{
    return QDir::toNativeSeparators(QDir(PathSecurity::appDataPath()).filePath(QStringLiteral("thumbnails")));
}

bool isManagedThumbnailPath(const QString &thumbnailPath)
{
    return PathSecurity::isInsideOrEqual(thumbnailPath, defaultThumbnailRoot());
}

QString orderByClause(const MediaLibraryQuery &query)
{
    const QString direction = query.ascending ? QStringLiteral("ASC") : QStringLiteral("DESC");

    switch (query.sortKey) {
    case MediaLibrarySortKey::Size:
        return QStringLiteral("file_size %1, file_name COLLATE NOCASE ASC, file_path ASC").arg(direction);
    case MediaLibrarySortKey::Modified:
        return QStringLiteral("modified_at %1, file_name COLLATE NOCASE ASC, file_path ASC").arg(direction);
    case MediaLibrarySortKey::Name:
    default:
        return QStringLiteral("file_name COLLATE NOCASE %1, file_path ASC").arg(direction);
    }
}

QString formatFileSize(qint64 bytes)
{
    static const QStringList units = {
        QStringLiteral("B"),
        QStringLiteral("KB"),
        QStringLiteral("MB"),
        QStringLiteral("GB"),
        QStringLiteral("TB"),
    };

    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }

    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < units.size() - 1) {
        value /= 1024.0;
        ++unitIndex;
    }

    const int precision = value < 10.0 ? 1 : 0;
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', precision), units.at(unitIndex));
}

QString formatDuration(const QVariant &durationValue)
{
    if (durationValue.isNull()) {
        return QStringLiteral("--:--");
    }

    const qint64 totalSeconds = durationValue.toLongLong() / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatResolution(const QVariant &widthValue, const QVariant &heightValue)
{
    if (widthValue.isNull() || heightValue.isNull()) {
        return QStringLiteral("-");
    }

    return QStringLiteral("%1 x %2").arg(widthValue.toInt()).arg(heightValue.toInt());
}

QString formatCodec(const QVariant &videoCodecValue, const QVariant &audioCodecValue)
{
    const QString videoCodec = videoCodecValue.toString();
    const QString audioCodec = audioCodecValue.toString();

    if (videoCodec.isEmpty() && audioCodec.isEmpty()) {
        return QStringLiteral("-");
    }
    if (audioCodec.isEmpty()) {
        return videoCodec;
    }
    if (videoCodec.isEmpty()) {
        return audioCodec;
    }

    return QStringLiteral("%1 / %2").arg(videoCodec, audioCodec);
}

QString formatBitrate(const QVariant &bitrateValue)
{
    if (bitrateValue.isNull()) {
        return QStringLiteral("-");
    }

    const qint64 bitsPerSecond = bitrateValue.toLongLong();
    if (bitsPerSecond <= 0) {
        return QStringLiteral("-");
    }

    if (bitsPerSecond >= 1000000) {
        return QStringLiteral("%1 Mbps").arg(QString::number(bitsPerSecond / 1000000.0, 'f', 1));
    }

    return QStringLiteral("%1 kbps").arg(QString::number(bitsPerSecond / 1000.0, 'f', 0));
}

QString formatFrameRate(const QVariant &frameRateValue)
{
    if (frameRateValue.isNull()) {
        return QStringLiteral("-");
    }

    const double framesPerSecond = frameRateValue.toDouble();
    if (framesPerSecond <= 0.0) {
        return QStringLiteral("-");
    }

    const double roundedValue = std::round(framesPerSecond);
    const int precision = std::abs(framesPerSecond - roundedValue) < 0.01 ? 0 : 2;
    return QStringLiteral("%1 fps").arg(QString::number(framesPerSecond, 'f', precision));
}

bool isAllowedReviewStatus(const QString &reviewStatus)
{
    return reviewStatus == QStringLiteral("unreviewed")
        || reviewStatus == QStringLiteral("reviewed")
        || reviewStatus == QStringLiteral("needs_followup");
}

QStringList normalizedTags(const QStringList &tags)
{
    QStringList result;
    QSet<QString> seen;

    for (const QString &tag : tags) {
        const QString trimmedTag = tag.trimmed();
        if (trimmedTag.isEmpty()) {
            continue;
        }

        const QString key = trimmedTag.toCaseFolded();
        if (seen.contains(key)) {
            continue;
        }

        seen.insert(key);
        result.append(trimmedTag);
    }

    return result;
}

MediaLibraryItem mediaLibraryItemFromQuery(const QSqlQuery &query)
{
    MediaLibraryItem item;
    item.id = query.value(0).toInt();
    item.fileName = query.value(1).toString();
    item.filePath = query.value(2).toString();
    item.fileSizeBytes = query.value(3).toLongLong();
    item.fileSize = formatFileSize(item.fileSizeBytes);
    item.modifiedAt = query.value(4).toString();
    item.durationMs = query.value(5).isNull()
        ? 0
        : query.value(5).toLongLong();
    item.duration = formatDuration(query.value(5));
    item.resolution = formatResolution(
        query.value(6),
        query.value(7));
    item.codec = formatCodec(
        query.value(8),
        query.value(9));
    item.bitrateBps = query.value(10).isNull()
        ? 0
        : query.value(10).toLongLong();
    item.bitrate = formatBitrate(query.value(10));
    item.frameRateValue = query.value(11).isNull()
        ? 0.0
        : query.value(11).toDouble();
    item.frameRate = formatFrameRate(query.value(11));
    item.description = query.value(12).toString();
    item.reviewStatus = query.value(13).toString();
    item.rating = query.value(14).toInt();
    const QString thumbnailPath = query.value(15).toString();
    item.thumbnailPath = isManagedThumbnailPath(thumbnailPath) ? thumbnailPath : QString();
    item.isFavorite = query.value(16).toInt() != 0;
    item.isDeleteCandidate = query.value(17).toInt() != 0;
    item.lastPositionMs = query.value(18).toLongLong();
    item.lastPlayedAt = query.value(19).toString();
    return item;
}

MediaFile mediaFileFromQuery(const QSqlQuery &query)
{
    MediaFile file;
    file.id = query.value(0).toInt();
    file.fileName = query.value(1).toString();
    file.filePath = query.value(2).toString();
    file.fileSizeBytes = query.value(3).toLongLong();
    file.modifiedAt = query.value(4).toString();
    file.durationMs = query.value(5).isNull() ? 0 : query.value(5).toLongLong();
    file.width = query.value(6).isNull() ? 0 : query.value(6).toInt();
    file.height = query.value(7).isNull() ? 0 : query.value(7).toInt();
    file.videoCodec = query.value(8).toString();
    file.audioCodec = query.value(9).toString();
    file.bitrateBps = query.value(10).isNull() ? 0 : query.value(10).toLongLong();
    file.frameRateValue = query.value(11).isNull() ? 0.0 : query.value(11).toDouble();
    file.description = query.value(12).toString();
    file.reviewStatus = query.value(13).toString();
    file.rating = query.value(14).toInt();
    file.thumbnailPath = query.value(15).toString();
    file.isFavorite = query.value(16).toInt() != 0;
    file.isDeleteCandidate = query.value(17).toInt() != 0;
    file.lastPositionMs = query.value(18).toLongLong();
    file.lastPlayedAt = query.value(19).toString();
    return file;
}
}

MediaRepository::MediaRepository(QSqlDatabase database)
    : m_database(std::move(database))
{
}

bool MediaRepository::upsertScanResult(
    const QString &rootPath,
    const QVector<ScannedMediaFile> &files,
    MediaUpsertResult *result)
{
    if (result) {
        result->scannedFileCount = files.size();
        result->upsertedFileCount = 0;
    }

    if (!ensureOpen()) {
        return false;
    }

    if (!m_database.transaction()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    if (!upsertScanRoot(rootPath)) {
        m_database.rollback();
        return false;
    }

    QSqlQuery upsertQuery(m_database);
    if (!prepareMediaFileUpsert(&upsertQuery)) {
        m_database.rollback();
        return false;
    }

    for (const ScannedMediaFile &file : files) {
        if (!upsertMediaFile(&upsertQuery, file)) {
            m_database.rollback();
            return false;
        }

        if (result) {
            ++result->upsertedFileCount;
        }
    }

    if (!m_database.commit()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

QVector<MediaFile> MediaRepository::fetchMediaFiles()
{
    return fetchMediaFiles(MediaLibraryQuery {});
}

QVector<MediaFile> MediaRepository::fetchMediaFiles(const MediaLibraryQuery &libraryQuery)
{
    QVector<MediaFile> files;

    if (!ensureOpen()) {
        return files;
    }

    const QString trimmedSearchText = libraryQuery.searchText.trimmed();
    QString sql = QStringLiteral(R"sql(
        SELECT
            id,
            file_name,
            file_path,
            file_size,
            modified_at,
            duration_ms,
            width,
            height,
            video_codec,
            audio_codec,
            bitrate,
            frame_rate,
            description,
            review_status,
            rating,
            thumbnail_path,
            is_favorite,
            is_delete_candidate,
            last_position_ms,
            last_played_at
        FROM media_files
    )sql");

    if (!trimmedSearchText.isEmpty()) {
        sql += QStringLiteral(R"sql(
        WHERE file_name LIKE ? ESCAPE '\' COLLATE NOCASE
           OR description LIKE ? ESCAPE '\' COLLATE NOCASE
    )sql");
    }

    sql += QStringLiteral(" ORDER BY %1").arg(orderByClause(libraryQuery));

    QSqlQuery query(m_database);
    query.prepare(sql);

    if (!trimmedSearchText.isEmpty()) {
        const QString pattern = escapedLikePattern(trimmedSearchText);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return files;
    }

    QVector<int> mediaIds;
    while (query.next()) {
        MediaFile file = mediaFileFromQuery(query);
        mediaIds.append(file.id);
        files.append(file);
    }

    query.finish();

    bool tagsOk = false;
    const QHash<int, QStringList> tagMap = tagsByMediaIds(mediaIds, &tagsOk);
    if (!tagsOk) {
        return {};
    }

    for (MediaFile &file : files) {
        file.tags = tagMap.value(file.id);
    }

    m_lastError.clear();
    return files;
}

MediaFile MediaRepository::fetchMediaFileById(int mediaId)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return {};
    }
    if (!ensureOpen()) {
        return {};
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        SELECT
            id,
            file_name,
            file_path,
            file_size,
            modified_at,
            duration_ms,
            width,
            height,
            video_codec,
            audio_codec,
            bitrate,
            frame_rate,
            description,
            review_status,
            rating,
            thumbnail_path,
            is_favorite,
            is_delete_candidate,
            last_position_ms,
            last_played_at
        FROM media_files
        WHERE id = ?
    )sql"));
    query.addBindValue(mediaId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return {};
    }
    if (!query.next()) {
        setLastError(QStringLiteral("Media item not found."));
        return {};
    }

    MediaFile file = mediaFileFromQuery(query);
    query.finish();
    bool tagsOk = false;
    const QHash<int, QStringList> tagMap = tagsByMediaIds({mediaId}, &tagsOk);
    if (!tagsOk) {
        return {};
    }
    file.tags = tagMap.value(mediaId);
    m_lastError.clear();
    return file;
}

QVector<MediaLibraryItem> MediaRepository::fetchLibraryItems()
{
    return fetchLibraryItems(MediaLibraryQuery {});
}

QVector<MediaLibraryItem> MediaRepository::fetchLibraryItems(const MediaLibraryQuery &libraryQuery)
{
    QVector<MediaLibraryItem> items;

    if (!ensureOpen()) {
        return items;
    }

    QSqlQuery query(m_database);
    QString sql = QStringLiteral(R"sql(
        SELECT
            id,
            file_name,
            file_path,
            file_size,
            modified_at,
            duration_ms,
            width,
            height,
            video_codec,
            audio_codec,
            bitrate,
            frame_rate,
            description,
            review_status,
            rating,
            thumbnail_path,
            is_favorite,
            is_delete_candidate,
            last_position_ms,
            last_played_at
        FROM media_files
    )sql");

    const QString searchText = libraryQuery.searchText.trimmed();
    if (!searchText.isEmpty()) {
        sql += QStringLiteral(R"sql(
            WHERE file_name LIKE ? ESCAPE '\' COLLATE NOCASE
               OR description LIKE ? ESCAPE '\' COLLATE NOCASE
        )sql");
    }
    sql += QStringLiteral(" ORDER BY %1").arg(orderByClause(libraryQuery));

    if (!query.prepare(sql)) {
        setLastError(query.lastError().text());
        return items;
    }

    if (!searchText.isEmpty()) {
        const QString pattern = escapedLikePattern(searchText);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return items;
    }

    QVector<int> mediaIds;
    while (query.next()) {
        MediaLibraryItem item = mediaLibraryItemFromQuery(query);
        mediaIds.append(item.id);
        items.append(item);
    }

    query.finish();

    bool tagsOk = false;
    const QHash<int, QStringList> tagMap = tagsByMediaIds(mediaIds, &tagsOk);
    if (!tagsOk) {
        return {};
    }

    for (MediaLibraryItem &item : items) {
        item.tags = tagMap.value(item.id);
    }

    m_lastError.clear();
    return items;
}

MediaLibraryItem MediaRepository::fetchMediaById(int mediaId)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return {};
    }

    if (!ensureOpen()) {
        return {};
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        SELECT
            id,
            file_name,
            file_path,
            file_size,
            modified_at,
            duration_ms,
            width,
            height,
            video_codec,
            audio_codec,
            bitrate,
            frame_rate,
            description,
            review_status,
            rating,
            thumbnail_path,
            is_favorite,
            is_delete_candidate,
            last_position_ms,
            last_played_at
        FROM media_files
        WHERE id = ?
    )sql"));
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return {};
    }
    if (!query.next()) {
        setLastError(QStringLiteral("Media row was not found."));
        return {};
    }

    MediaLibraryItem item = mediaLibraryItemFromQuery(query);
    query.finish();

    bool tagsOk = false;
    const QHash<int, QStringList> tagMap = tagsByMediaIds({mediaId}, &tagsOk);
    if (!tagsOk) {
        return {};
    }
    item.tags = tagMap.value(mediaId);

    m_lastError.clear();
    return item;
}

bool MediaRepository::renameMediaFile(int mediaId, const ScannedMediaFile &file)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }

    if (file.fileName.trimmed().isEmpty() || file.filePath.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Renamed file information is incomplete."));
        return false;
    }

    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET
            file_name = ?,
            file_path = ?,
            file_extension = ?,
            file_size = ?,
            modified_at = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    query.addBindValue(file.fileName);
    query.addBindValue(file.filePath);
    query.addBindValue(file.fileExtension);
    query.addBindValue(file.fileSize);
    query.addBindValue(file.modifiedAt);
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::setMediaFavorite(int mediaId, bool enabled)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET is_favorite = ?, updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    query.addBindValue(enabled ? 1 : 0);
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::setMediaDeleteCandidate(int mediaId, bool enabled)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET is_delete_candidate = ?, updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    query.addBindValue(enabled ? 1 : 0);
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::updatePlaybackPosition(int mediaId, qint64 positionMs, const QString &playedAt)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }
    if (positionMs < 0) {
        setLastError(QStringLiteral("Playback position cannot be negative."));
        return false;
    }
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET
            last_position_ms = ?,
            last_played_at = CASE WHEN ? = '' THEN datetime('now') ELSE ? END,
            updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    query.addBindValue(positionMs);
    query.addBindValue(playedAt);
    query.addBindValue(playedAt);
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::resetLibraryData()
{
    if (!ensureOpen()) {
        return false;
    }

    if (!m_database.transaction()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    const QStringList statements = {
        QStringLiteral("DELETE FROM snapshots"),
        QStringLiteral("DELETE FROM media_tags"),
        QStringLiteral("DELETE FROM media_files"),
        QStringLiteral("DELETE FROM tags"),
        QStringLiteral("DELETE FROM scan_roots"),
    };

    for (const QString &statement : statements) {
        QSqlQuery query(m_database);
        if (!query.exec(statement)) {
            setLastError(query.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::updateMediaMetadata(int mediaId, const MediaMetadata &metadata)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }

    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET
            duration_ms = ?,
            bitrate = ?,
            frame_rate = ?,
            width = ?,
            height = ?,
            video_codec = ?,
            audio_codec = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    query.addBindValue(metadata.durationMs > 0 ? QVariant(metadata.durationMs) : QVariant());
    query.addBindValue(metadata.bitrate > 0 ? QVariant(metadata.bitrate) : QVariant());
    query.addBindValue(metadata.frameRate > 0.0 ? QVariant(metadata.frameRate) : QVariant());
    query.addBindValue(metadata.width > 0 ? QVariant(metadata.width) : QVariant());
    query.addBindValue(metadata.height > 0 ? QVariant(metadata.height) : QVariant());
    query.addBindValue(metadata.videoCodec);
    query.addBindValue(metadata.audioCodec);
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::updateMediaDetails(
    int mediaId,
    const QString &description,
    const QString &reviewStatus,
    int rating,
    const QStringList &tags)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }
    if (rating < 0 || rating > 5) {
        setLastError(QStringLiteral("Rating must be between 0 and 5."));
        return false;
    }
    if (!isAllowedReviewStatus(reviewStatus)) {
        setLastError(QStringLiteral("Invalid review status."));
        return false;
    }
    if (!ensureOpen()) {
        return false;
    }

    if (!m_database.transaction()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    QSqlQuery updateQuery(m_database);
    updateQuery.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET
            description = ?,
            review_status = ?,
            rating = ?,
            updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    updateQuery.addBindValue(description);
    updateQuery.addBindValue(reviewStatus);
    updateQuery.addBindValue(rating);
    updateQuery.addBindValue(mediaId);

    if (!updateQuery.exec()) {
        setLastError(updateQuery.lastError().text());
        m_database.rollback();
        return false;
    }
    if (updateQuery.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        m_database.rollback();
        return false;
    }

    QSqlQuery deleteQuery(m_database);
    deleteQuery.prepare(QStringLiteral("DELETE FROM media_tags WHERE media_id = ?"));
    deleteQuery.addBindValue(mediaId);
    if (!deleteQuery.exec()) {
        setLastError(deleteQuery.lastError().text());
        m_database.rollback();
        return false;
    }

    const QStringList tagNames = normalizedTags(tags);
    for (const QString &tagName : tagNames) {
        const int tagId = tagIdForName(tagName);
        if (tagId <= 0) {
            m_database.rollback();
            return false;
        }

        QSqlQuery linkQuery(m_database);
        linkQuery.prepare(QStringLiteral(R"sql(
            INSERT OR IGNORE INTO media_tags (media_id, tag_id)
            VALUES (?, ?)
        )sql"));
        linkQuery.addBindValue(mediaId);
        linkQuery.addBindValue(tagId);
        if (!linkQuery.exec()) {
            setLastError(linkQuery.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaRepository::addSnapshot(int mediaId, const QString &imagePath, qint64 timestampMs, int *snapshotId)
{
    if (snapshotId) {
        *snapshotId = -1;
    }
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }
    if (imagePath.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Snapshot image path is empty."));
        return false;
    }
    if (timestampMs < 0) {
        setLastError(QStringLiteral("Snapshot timestamp cannot be negative."));
        return false;
    }
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO snapshots (media_id, image_path, timestamp_ms)
        VALUES (?, ?, ?)
    )sql"));
    query.addBindValue(mediaId);
    query.addBindValue(imagePath);
    query.addBindValue(timestampMs);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    if (snapshotId) {
        *snapshotId = query.lastInsertId().toInt();
    }
    m_lastError.clear();
    return true;
}

QVector<SnapshotItem> MediaRepository::fetchSnapshotsForMedia(int mediaId)
{
    QVector<SnapshotItem> snapshots;
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return snapshots;
    }
    if (!ensureOpen()) {
        return snapshots;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        SELECT id, media_id, image_path, timestamp_ms, created_at
        FROM snapshots
        WHERE media_id = ?
        ORDER BY id DESC
    )sql"));
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return snapshots;
    }

    while (query.next()) {
        SnapshotItem snapshot;
        snapshot.id = query.value(QStringLiteral("id")).toInt();
        snapshot.mediaId = query.value(QStringLiteral("media_id")).toInt();
        snapshot.imagePath = query.value(QStringLiteral("image_path")).toString();
        snapshot.timestampMs = query.value(QStringLiteral("timestamp_ms")).toLongLong();
        snapshot.createdAt = query.value(QStringLiteral("created_at")).toString();
        snapshots.append(snapshot);
    }

    m_lastError.clear();
    return snapshots;
}

bool MediaRepository::setMediaThumbnailPath(int mediaId, const QString &imagePath)
{
    if (mediaId <= 0) {
        setLastError(QStringLiteral("Invalid media id."));
        return false;
    }
    if (imagePath.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Thumbnail image path is empty."));
        return false;
    }
    if (!ensureOpen()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        UPDATE media_files
        SET thumbnail_path = ?, updated_at = datetime('now')
        WHERE id = ?
    )sql"));
    query.addBindValue(imagePath);
    query.addBindValue(mediaId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setLastError(QStringLiteral("Media row was not found."));
        return false;
    }

    m_lastError.clear();
    return true;
}

QVector<ThumbnailBackfillItem> MediaRepository::fetchThumbnailBackfillItems()
{
    QVector<ThumbnailBackfillItem> items;
    if (!ensureOpen()) {
        return items;
    }

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(R"sql(
        SELECT
            media_files.id AS media_id,
            COALESCE(latest_snapshots.image_path, media_files.thumbnail_path) AS source_image_path,
            media_files.thumbnail_path AS existing_thumbnail_path
        FROM media_files
        LEFT JOIN snapshots AS latest_snapshots
            ON latest_snapshots.id = (
                SELECT snapshots.id
                FROM snapshots
                WHERE snapshots.media_id = media_files.id
                ORDER BY snapshots.id DESC
                LIMIT 1
            )
        WHERE
            (latest_snapshots.image_path IS NOT NULL AND latest_snapshots.image_path <> '')
            OR (media_files.thumbnail_path IS NOT NULL AND media_files.thumbnail_path <> '')
        ORDER BY media_files.file_name COLLATE NOCASE, media_files.file_path
    )sql"))) {
        setLastError(query.lastError().text());
        return items;
    }

    while (query.next()) {
        ThumbnailBackfillItem item;
        item.mediaId = query.value(QStringLiteral("media_id")).toInt();
        item.sourceImagePath = query.value(QStringLiteral("source_image_path")).toString();
        item.existingThumbnailPath = query.value(QStringLiteral("existing_thumbnail_path")).toString();
        items.append(item);
    }

    m_lastError.clear();
    return items;
}

QString MediaRepository::lastError() const
{
    return m_lastError;
}

bool MediaRepository::upsertScanRoot(const QString &rootPath)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO scan_roots (root_path, is_enabled, last_scanned_at)
        VALUES (?, 1, datetime('now'))
        ON CONFLICT(root_path) DO UPDATE SET
            is_enabled = 1,
            last_scanned_at = datetime('now')
    )sql"));
    query.addBindValue(rootPath);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool MediaRepository::prepareMediaFileUpsert(QSqlQuery *query)
{
    if (!query) {
        setLastError(QStringLiteral("Media upsert query is unavailable."));
        return false;
    }

    if (!query->prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name,
            file_path,
            file_extension,
            file_size,
            modified_at
        )
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(file_path) DO UPDATE SET
            file_name = excluded.file_name,
            file_extension = excluded.file_extension,
            file_size = excluded.file_size,
            modified_at = excluded.modified_at,
            updated_at = datetime('now')
        WHERE
            media_files.file_name IS NOT excluded.file_name
            OR media_files.file_extension IS NOT excluded.file_extension
            OR media_files.file_size IS NOT excluded.file_size
            OR media_files.modified_at IS NOT excluded.modified_at
    )sql"))) {
        setLastError(query->lastError().text());
        return false;
    }

    return true;
}

bool MediaRepository::upsertMediaFile(QSqlQuery *query, const ScannedMediaFile &file)
{
    if (!query) {
        setLastError(QStringLiteral("Media upsert query is unavailable."));
        return false;
    }

    query->bindValue(0, file.fileName);
    query->bindValue(1, file.filePath);
    query->bindValue(2, file.fileExtension);
    query->bindValue(3, file.fileSize);
    query->bindValue(4, file.modifiedAt);

    if (!query->exec()) {
        setLastError(query->lastError().text());
        return false;
    }

    return true;
}

QHash<int, QStringList> MediaRepository::tagsByMediaIds(const QVector<int> &mediaIds, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    QHash<int, QStringList> tags;
    if (mediaIds.isEmpty()) {
        if (ok) {
            *ok = true;
        }
        return tags;
    }

    constexpr int ChunkSize = 900;
    const int mediaIdCount = static_cast<int>(mediaIds.size());
    for (int offset = 0; offset < mediaIdCount; offset += ChunkSize) {
        const int chunkSize = std::min(ChunkSize, mediaIdCount - offset);
        QStringList placeholders;
        placeholders.reserve(chunkSize);
        for (int index = 0; index < chunkSize; ++index) {
            placeholders.append(QStringLiteral("?"));
        }

        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(R"sql(
            SELECT media_tags.media_id, tags.name
            FROM media_tags
            INNER JOIN tags ON media_tags.tag_id = tags.id
            WHERE media_tags.media_id IN (%1)
            ORDER BY media_tags.media_id, tags.name COLLATE NOCASE, tags.name
        )sql").arg(placeholders.join(QLatin1Char(','))));

        for (int index = 0; index < chunkSize; ++index) {
            query.addBindValue(mediaIds.at(offset + index));
        }

        if (!query.exec()) {
            setLastError(query.lastError().text());
            return tags;
        }

        while (query.next()) {
            tags[query.value(0).toInt()].append(query.value(1).toString());
        }
    }

    if (ok) {
        *ok = true;
    }
    return tags;
}

int MediaRepository::tagIdForName(const QString &tagName)
{
    QSqlQuery selectQuery(m_database);
    selectQuery.prepare(QStringLiteral("SELECT id FROM tags WHERE name = ? COLLATE NOCASE LIMIT 1"));
    selectQuery.addBindValue(tagName);

    if (!selectQuery.exec()) {
        setLastError(selectQuery.lastError().text());
        return -1;
    }

    if (selectQuery.next()) {
        return selectQuery.value(0).toInt();
    }

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare(QStringLiteral("INSERT INTO tags (name) VALUES (?)"));
    insertQuery.addBindValue(tagName);
    if (!insertQuery.exec()) {
        setLastError(insertQuery.lastError().text());
        return -1;
    }

    return insertQuery.lastInsertId().toInt();
}

bool MediaRepository::ensureOpen()
{
    if (!m_database.isOpen()) {
        setLastError(QStringLiteral("Database connection is not open."));
        return false;
    }

    return true;
}

void MediaRepository::setLastError(const QString &error)
{
    m_lastError = error;
}

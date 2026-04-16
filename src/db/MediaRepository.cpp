#include "MediaRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {
QString escapedLikePattern(QString text)
{
    text.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    text.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    text.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return QStringLiteral("%%1%").arg(text);
}

QString orderByClause(const MediaLibraryQuery &query)
{
    const QString direction = query.ascending ? QStringLiteral("ASC") : QStringLiteral("DESC");

    switch (query.sortKey) {
    case MediaLibrarySortKey::Size:
        return QStringLiteral("file_size %1, lower(file_name) ASC, file_path ASC").arg(direction);
    case MediaLibrarySortKey::Modified:
        return QStringLiteral("modified_at %1, lower(file_name) ASC, file_path ASC").arg(direction);
    case MediaLibrarySortKey::Name:
    default:
        return QStringLiteral("lower(file_name) %1, file_path ASC").arg(direction);
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

    for (const ScannedMediaFile &file : files) {
        if (!upsertMediaFile(file)) {
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
            description,
            review_status,
            rating,
            thumbnail_path
        FROM media_files
    )sql");

    if (!trimmedSearchText.isEmpty()) {
        sql += QStringLiteral(R"sql(
        WHERE lower(file_name) LIKE lower(?) ESCAPE '\'
           OR lower(description) LIKE lower(?) ESCAPE '\'
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
        return items;
    }

    while (query.next()) {
        MediaLibraryItem item;
        item.id = query.value(QStringLiteral("id")).toInt();
        item.fileName = query.value(QStringLiteral("file_name")).toString();
        item.filePath = query.value(QStringLiteral("file_path")).toString();
        item.fileSizeBytes = query.value(QStringLiteral("file_size")).toLongLong();
        item.fileSize = formatFileSize(item.fileSizeBytes);
        item.modifiedAt = query.value(QStringLiteral("modified_at")).toString();
        item.durationMs = query.value(QStringLiteral("duration_ms")).isNull()
            ? 0
            : query.value(QStringLiteral("duration_ms")).toLongLong();
        item.duration = formatDuration(query.value(QStringLiteral("duration_ms")));
        item.resolution = formatResolution(
            query.value(QStringLiteral("width")),
            query.value(QStringLiteral("height")));
        item.codec = formatCodec(
            query.value(QStringLiteral("video_codec")),
            query.value(QStringLiteral("audio_codec")));
        item.description = query.value(QStringLiteral("description")).toString();
        item.reviewStatus = query.value(QStringLiteral("review_status")).toString();
        item.rating = query.value(QStringLiteral("rating")).toInt();
        item.thumbnailPath = query.value(QStringLiteral("thumbnail_path")).toString();
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

bool MediaRepository::upsertMediaFile(const ScannedMediaFile &file)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
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
    )sql"));
    query.addBindValue(file.fileName);
    query.addBindValue(file.filePath);
    query.addBindValue(file.fileExtension);
    query.addBindValue(file.fileSize);
    query.addBindValue(file.modifiedAt);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
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

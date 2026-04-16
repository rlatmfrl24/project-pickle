#pragma once

#include "media/MediaTypes.h"

#include <QSqlDatabase>
#include <QString>
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
    QString lastError() const;

private:
    bool upsertScanRoot(const QString &rootPath);
    bool upsertMediaFile(const ScannedMediaFile &file);
    bool ensureOpen();
    void setLastError(const QString &error);

    QSqlDatabase m_database;
    QString m_lastError;
};

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

struct ScannedMediaFile {
    QString fileName;
    QString filePath;
    QString fileExtension;
    qint64 fileSize = 0;
    QString modifiedAt;
};

struct DirectoryScanResult {
    QString rootPath;
    QVector<ScannedMediaFile> files;
    QString errorMessage;
    bool succeeded = false;
};

struct MediaLibraryItem {
    int id = 0;
    QString fileName;
    QString filePath;
    QString fileSize;
    qint64 fileSizeBytes = 0;
    QString duration = QStringLiteral("--:--");
    qint64 durationMs = 0;
    QString resolution = QStringLiteral("-");
    QString codec = QStringLiteral("-");
    QString description;
    QStringList tags;
    QString reviewStatus = QStringLiteral("unreviewed");
    int rating = 0;
    QString modifiedAt;
    QString thumbnailPath;
};

struct MediaUpsertResult {
    int scannedFileCount = 0;
    int upsertedFileCount = 0;
};

enum class MediaLibrarySortKey {
    Name,
    Size,
    Modified
};

struct MediaLibraryQuery {
    QString searchText;
    MediaLibrarySortKey sortKey = MediaLibrarySortKey::Name;
    bool ascending = true;
};

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
    int visitedFileCount = 0;
    int matchedFileCount = 0;
    bool canceled = false;
    bool succeeded = false;
};

struct ScanProgress {
    QString rootPath;
    QString currentPath;
    int visitedFileCount = 0;
    int matchedFileCount = 0;
    bool canceled = false;
};

struct FileRenameResult {
    ScannedMediaFile file;
    QString errorMessage;
    bool succeeded = false;
};

struct BatchRenameRule {
    QString prefix;
    QString suffix;
    bool numberingEnabled = false;
    int numberStart = 1;
    int numberPadding = 2;
};

struct BatchRenamePreviewItem {
    int mediaId = 0;
    QString currentName;
    QString currentPath;
    QString newBaseName;
    QString newName;
    QString newPath;
    QString status;
    bool runnable = false;
    bool succeeded = false;
};

struct BatchRenamePreviewResult {
    QVector<BatchRenamePreviewItem> items;
    QString status;
    int runnableCount = 0;
    int invalidCount = 0;
    bool succeeded = false;
};

struct BatchRenameExecutionResult {
    QVector<BatchRenamePreviewItem> items;
    QString status;
    int successCount = 0;
    int failureCount = 0;
    int skippedCount = 0;
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
    QString bitrate = QStringLiteral("-");
    qint64 bitrateBps = 0;
    QString frameRate = QStringLiteral("-");
    double frameRateValue = 0.0;
    QString description;
    QStringList tags;
    QString reviewStatus = QStringLiteral("unreviewed");
    int rating = 0;
    QString modifiedAt;
    QString thumbnailPath;
    bool isFavorite = false;
    bool isDeleteCandidate = false;
    qint64 lastPositionMs = 0;
    QString lastPlayedAt;
};

struct MediaFile {
    int id = 0;
    QString fileName;
    QString filePath;
    qint64 fileSizeBytes = 0;
    QString modifiedAt;
    qint64 durationMs = 0;
    int width = 0;
    int height = 0;
    QString videoCodec;
    QString audioCodec;
    qint64 bitrateBps = 0;
    double frameRateValue = 0.0;
    QString description;
    QStringList tags;
    QString reviewStatus = QStringLiteral("unreviewed");
    int rating = 0;
    QString thumbnailPath;
    bool isFavorite = false;
    bool isDeleteCandidate = false;
    qint64 lastPositionMs = 0;
    QString lastPlayedAt;
};

struct MediaUpsertResult {
    int scannedFileCount = 0;
    int upsertedFileCount = 0;
};

struct MediaMetadata {
    qint64 durationMs = 0;
    qint64 bitrate = 0;
    double frameRate = 0.0;
    int width = 0;
    int height = 0;
    QString videoCodec;
    QString audioCodec;
};

struct MetadataExtractionResult {
    MediaMetadata metadata;
    QString errorMessage;
    bool canceled = false;
    bool succeeded = false;
};

struct SnapshotItem {
    int id = 0;
    int mediaId = 0;
    QString imagePath;
    qint64 timestampMs = 0;
    QString createdAt;
};

struct SnapshotCaptureRequest {
    int mediaId = 0;
    QString filePath;
    QString ffmpegProgram;
    QString outputRootPath;
    QString thumbnailRootPath;
    int thumbnailMaxWidth = 320;
    qint64 timestampMs = 0;
};

struct SnapshotCaptureResult {
    int mediaId = 0;
    QString imagePath;
    QString thumbnailPath;
    qint64 timestampMs = 0;
    QString errorMessage;
    bool canceled = false;
    bool succeeded = false;
};

struct ThumbnailGenerationRequest {
    int mediaId = 0;
    QString sourceImagePath;
    QString outputRootPath;
    int maxWidth = 320;
    int quality = 82;
};

struct ThumbnailGenerationResult {
    int mediaId = 0;
    QString thumbnailPath;
    QString errorMessage;
    bool canceled = false;
    bool skipped = false;
    bool succeeded = false;
};

struct ThumbnailBackfillItem {
    int mediaId = 0;
    QString sourceImagePath;
    QString existingThumbnailPath;
};

struct AppSettings {
    QString ffprobePath;
    QString ffmpegPath;
    bool showThumbnails = true;
    QString sortKey = QStringLiteral("name");
    bool sortAscending = true;
    QString lastOpenFolder;
    double playerVolume = 0.64;
    bool playerMuted = false;
};

struct ExternalToolStatus {
    QString toolName;
    QString configuredPath;
    QString resolvedProgram;
    QString versionText;
    QString errorMessage;
    bool configuredPathUsed = false;
    bool available = false;
    bool succeeded = false;
};

struct WorkEvent {
    QString timestamp;
    QString kind;
    QString message;
    bool warning = false;
};

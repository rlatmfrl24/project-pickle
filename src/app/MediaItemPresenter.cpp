#include "app/MediaItemPresenter.h"

#include "core/PathSecurity.h"

#include <QDir>
#include <QStringList>

#include <cmath>

namespace {
QString defaultThumbnailRoot()
{
    return QDir::toNativeSeparators(QDir(PathSecurity::appDataPath()).filePath(QStringLiteral("thumbnails")));
}

bool isManagedThumbnailPath(const QString &thumbnailPath)
{
    return PathSecurity::isInsideOrEqual(thumbnailPath, defaultThumbnailRoot());
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

QString formatDuration(qint64 durationMs)
{
    if (durationMs <= 0) {
        return QStringLiteral("--:--");
    }
    const qint64 totalSeconds = durationMs / 1000;
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

QString formatResolution(int width, int height)
{
    return width > 0 && height > 0
        ? QStringLiteral("%1 x %2").arg(width).arg(height)
        : QStringLiteral("-");
}

QString formatCodec(const QString &videoCodec, const QString &audioCodec)
{
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

QString formatBitrate(qint64 bitsPerSecond)
{
    if (bitsPerSecond <= 0) {
        return QStringLiteral("-");
    }
    if (bitsPerSecond >= 1000000) {
        return QStringLiteral("%1 Mbps").arg(QString::number(bitsPerSecond / 1000000.0, 'f', 1));
    }
    return QStringLiteral("%1 kbps").arg(QString::number(bitsPerSecond / 1000.0, 'f', 0));
}

QString formatFrameRate(double framesPerSecond)
{
    if (framesPerSecond <= 0.0) {
        return QStringLiteral("-");
    }
    const double roundedValue = std::round(framesPerSecond);
    const int precision = std::abs(framesPerSecond - roundedValue) < 0.01 ? 0 : 2;
    return QStringLiteral("%1 fps").arg(QString::number(framesPerSecond, 'f', precision));
}
}

MediaLibraryItem MediaItemPresenter::present(const MediaFile &file)
{
    MediaLibraryItem item;
    item.id = file.id;
    item.fileName = file.fileName;
    item.filePath = file.filePath;
    item.fileSizeBytes = file.fileSizeBytes;
    item.fileSize = formatFileSize(file.fileSizeBytes);
    item.modifiedAt = file.modifiedAt;
    item.durationMs = file.durationMs;
    item.duration = formatDuration(file.durationMs);
    item.resolution = formatResolution(file.width, file.height);
    item.codec = formatCodec(file.videoCodec, file.audioCodec);
    item.bitrateBps = file.bitrateBps;
    item.bitrate = formatBitrate(file.bitrateBps);
    item.frameRateValue = file.frameRateValue;
    item.frameRate = formatFrameRate(file.frameRateValue);
    item.description = file.description;
    item.tags = file.tags;
    item.reviewStatus = file.reviewStatus;
    item.rating = file.rating;
    item.thumbnailPath = isManagedThumbnailPath(file.thumbnailPath) ? file.thumbnailPath : QString();
    item.isFavorite = file.isFavorite;
    item.isDeleteCandidate = file.isDeleteCandidate;
    item.lastPositionMs = file.lastPositionMs;
    item.lastPlayedAt = file.lastPlayedAt;
    return item;
}

QVector<MediaLibraryItem> MediaItemPresenter::present(const QVector<MediaFile> &files)
{
    QVector<MediaLibraryItem> items;
    items.reserve(files.size());
    for (const MediaFile &file : files) {
        items.append(present(file));
    }
    return items;
}

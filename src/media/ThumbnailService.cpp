#include "ThumbnailService.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QStandardPaths>

namespace {
QString thumbnailFileName(int mediaId, const QFileInfo &sourceInfo, int maxWidth)
{
    return QStringLiteral("%1_%2_w%3.jpg")
        .arg(mediaId)
        .arg(sourceInfo.completeBaseName())
        .arg(maxWidth);
}
}

ThumbnailGenerationResult ThumbnailService::generate(
    const ThumbnailGenerationRequest &request,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    ThumbnailGenerationResult result;
    result.mediaId = request.mediaId;

    if (cancellationToken && cancellationToken->isCancellationRequested()) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Operation canceled.");
        return result;
    }

    if (request.mediaId <= 0) {
        result.errorMessage = QStringLiteral("Invalid media id.");
        return result;
    }

    const QFileInfo sourceInfo(request.sourceImagePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        result.errorMessage = QStringLiteral("Thumbnail source image does not exist.");
        return result;
    }

    if (request.outputRootPath.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Thumbnail output folder is not available.");
        return result;
    }

    const int maxWidth = request.maxWidth > 0 ? request.maxWidth : 320;
    const int quality = request.quality >= 0 && request.quality <= 100 ? request.quality : 82;

    QDir outputRoot(request.outputRootPath);
    const QString mediaFolderName = QString::number(request.mediaId);
    if (!outputRoot.mkpath(mediaFolderName)) {
        result.errorMessage = QStringLiteral("Thumbnail folder could not be created.");
        return result;
    }

    QDir mediaOutputDir(outputRoot.filePath(mediaFolderName));
    const QString outputPath = mediaOutputDir.filePath(thumbnailFileName(request.mediaId, sourceInfo, maxWidth));
    if (isUsableThumbnail(outputPath, maxWidth)) {
        result.thumbnailPath = QDir::toNativeSeparators(QFileInfo(outputPath).absoluteFilePath());
        result.skipped = true;
        result.succeeded = true;
        return result;
    }

    QImageReader reader(sourceInfo.absoluteFilePath());
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        result.errorMessage = reader.errorString().isEmpty()
            ? QStringLiteral("Thumbnail source image could not be read.")
            : reader.errorString();
        return result;
    }

    if (cancellationToken && cancellationToken->isCancellationRequested()) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Operation canceled.");
        return result;
    }

    if (image.width() > maxWidth) {
        image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }

    QImageWriter writer(outputPath, "jpg");
    writer.setQuality(quality);
    if (!writer.write(image)) {
        result.errorMessage = writer.errorString().isEmpty()
            ? QStringLiteral("Thumbnail image could not be written.")
            : writer.errorString();
        return result;
    }

    result.thumbnailPath = QDir::toNativeSeparators(QFileInfo(outputPath).absoluteFilePath());
    result.succeeded = true;
    return result;
}

QString ThumbnailService::defaultThumbnailRoot()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + QStringLiteral("/.pickle");
    }

    return QDir::toNativeSeparators(QDir(appDataPath).filePath(QStringLiteral("thumbnails")));
}

bool ThumbnailService::clearThumbnailRoot(const QString &rootPath, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    if (rootPath.trimmed().isEmpty()) {
        return true;
    }

    QDir thumbnailRoot(rootPath);
    if (!thumbnailRoot.exists()) {
        return true;
    }

    if (!thumbnailRoot.removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Thumbnail folder could not be removed.");
        }
        return false;
    }

    return true;
}

bool ThumbnailService::isUsableThumbnail(const QString &thumbnailPath, int maxWidth)
{
    if (thumbnailPath.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo thumbnailInfo(thumbnailPath);
    if (!thumbnailInfo.exists() || !thumbnailInfo.isFile() || thumbnailInfo.size() <= 0) {
        return false;
    }

    QImageReader reader(thumbnailInfo.absoluteFilePath());
    const QSize size = reader.size();
    return size.isValid() && size.width() > 0 && size.width() <= maxWidth;
}

#include "SnapshotService.h"

#include "core/PathSecurity.h"
#include "core/ProcessRunner.h"
#include "media/ThumbnailService.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int FfmpegTimeoutMs = 60000;

QString captureFileName(int mediaId, qint64 timestampMs)
{
    return QStringLiteral("%1_%2_%3.jpg")
        .arg(mediaId)
        .arg(timestampMs)
        .arg(QUuid::createUuid().toString(QUuid::Id128));
}

}

SnapshotCaptureResult SnapshotService::capture(
    const SnapshotCaptureRequest &request,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    SnapshotCaptureResult result;
    result.mediaId = request.mediaId;
    result.timestampMs = std::max<qint64>(0, request.timestampMs);

    if (cancellationToken && cancellationToken->isCancellationRequested()) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Snapshot canceled.");
        return result;
    }

    if (request.mediaId <= 0) {
        result.errorMessage = QStringLiteral("Invalid media id.");
        return result;
    }

    const QFileInfo mediaInfo(request.filePath);
    if (!mediaInfo.exists() || !mediaInfo.isFile()) {
        result.errorMessage = QStringLiteral("File does not exist.");
        return result;
    }

    if (request.outputRootPath.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Snapshot output folder is not available.");
        return result;
    }

    QDir outputRoot(request.outputRootPath);
    const QString mediaFolderName = QString::number(request.mediaId);
    if (!outputRoot.mkpath(mediaFolderName)) {
        result.errorMessage = QStringLiteral("Snapshot folder could not be created.");
        return result;
    }

    QDir mediaOutputDir(outputRoot.filePath(mediaFolderName));
    const QString outputPath = mediaOutputDir.filePath(captureFileName(request.mediaId, result.timestampMs));
    const QString timestampSeconds = QString::number(result.timestampMs / 1000.0, 'f', 3);

    ProcessRunner processRunner;
    const QString ffmpegProgram = request.ffmpegProgram.trimmed().isEmpty()
        ? QStringLiteral("ffmpeg")
        : request.ffmpegProgram.trimmed();
    const ProcessResult processResult = processRunner.run(
        ffmpegProgram,
        {
            QStringLiteral("-hide_banner"),
            QStringLiteral("-loglevel"),
            QStringLiteral("error"),
            QStringLiteral("-y"),
            QStringLiteral("-ss"),
            timestampSeconds,
            QStringLiteral("-i"),
            mediaInfo.absoluteFilePath(),
            QStringLiteral("-frames:v"),
            QStringLiteral("1"),
            QStringLiteral("-q:v"),
            QStringLiteral("2"),
            outputPath,
        },
        FfmpegTimeoutMs,
        cancellationToken);

    if (processResult.canceled) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Snapshot canceled.");
        return result;
    }

    if (processResult.timedOut) {
        result.errorMessage = QStringLiteral("ffmpeg timed out.");
        return result;
    }

    if (!processResult.succeeded) {
        result.errorMessage = processResult.errorMessage.isEmpty()
            ? QStringLiteral("ffmpeg failed.")
            : processResult.errorMessage;
        if (result.errorMessage.contains(QStringLiteral("could not be started"))) {
            result.errorMessage = QStringLiteral("ffmpeg could not be started. Check the configured ffmpeg path or PATH.");
        }
        return result;
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists() || !outputInfo.isFile() || outputInfo.size() <= 0) {
        result.errorMessage = QStringLiteral("ffmpeg did not create a valid snapshot image.");
        return result;
    }

    result.imagePath = QDir::toNativeSeparators(outputInfo.absoluteFilePath());

    ThumbnailGenerationRequest thumbnailRequest;
    thumbnailRequest.mediaId = request.mediaId;
    thumbnailRequest.sourceImagePath = result.imagePath;
    thumbnailRequest.outputRootPath = request.thumbnailRootPath.trimmed().isEmpty()
        ? ThumbnailService::defaultThumbnailRoot()
        : request.thumbnailRootPath;
    thumbnailRequest.maxWidth = request.thumbnailMaxWidth > 0 ? request.thumbnailMaxWidth : 320;
    ThumbnailService thumbnailService;
    const ThumbnailGenerationResult thumbnailResult = thumbnailService.generate(thumbnailRequest, cancellationToken);
    if (!thumbnailResult.succeeded) {
        result.canceled = thumbnailResult.canceled;
        result.errorMessage = thumbnailResult.canceled
            ? QStringLiteral("Snapshot canceled.")
            : QStringLiteral("Snapshot captured, but thumbnail cache failed: %1").arg(thumbnailResult.errorMessage);
        return result;
    }

    result.thumbnailPath = thumbnailResult.thumbnailPath;
    result.succeeded = true;
    return result;
}

QString SnapshotService::defaultSnapshotRoot()
{
    return QDir::toNativeSeparators(QDir(PathSecurity::appDataPath()).filePath(QStringLiteral("snapshots")));
}

bool SnapshotService::clearSnapshotRoot(const QString &rootPath, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    if (rootPath.trimmed().isEmpty()) {
        return true;
    }

    if (!PathSecurity::samePath(rootPath, defaultSnapshotRoot())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Refusing to clear a folder outside the managed snapshot root.");
        }
        return false;
    }

    QDir snapshotRoot(rootPath);
    if (!snapshotRoot.exists()) {
        return true;
    }

    if (!snapshotRoot.removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Snapshot folder could not be removed.");
        }
        return false;
    }

    return true;
}

#include "ScanService.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace {
QString normalizedPath(const QFileInfo &fileInfo)
{
    QString path = fileInfo.canonicalFilePath();
    if (path.isEmpty()) {
        path = fileInfo.absoluteFilePath();
    }

    return QDir::toNativeSeparators(path);
}

QString normalizedExtension(const QFileInfo &fileInfo)
{
    const QString suffix = fileInfo.suffix().toLower();
    return suffix.isEmpty() ? QString() : QStringLiteral(".%1").arg(suffix);
}
}

DirectoryScanResult ScanService::scanDirectory(
    const QString &rootPath,
    const std::shared_ptr<CancellationToken> &cancellationToken,
    const ProgressCallback &progressCallback) const
{
    DirectoryScanResult result;

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        result.errorMessage = QStringLiteral("Scan root is not a directory: %1").arg(rootPath);
        return result;
    }

    result.rootPath = normalizedPath(rootInfo);

    auto reportProgress = [&](const QString &currentPath, bool canceled = false) {
        if (!progressCallback) {
            return;
        }

        ScanProgress progress;
        progress.rootPath = result.rootPath;
        progress.currentPath = currentPath;
        progress.visitedFileCount = result.visitedFileCount;
        progress.matchedFileCount = result.matchedFileCount;
        progress.canceled = canceled;
        progressCallback(progress);
    };

    if (cancellationToken && cancellationToken->isCancellationRequested()) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Scan canceled.");
        reportProgress(result.rootPath, true);
        return result;
    }

    QDirIterator iterator(
        result.rootPath,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        if (cancellationToken && cancellationToken->isCancellationRequested()) {
            result.files.clear();
            result.canceled = true;
            result.errorMessage = QStringLiteral("Scan canceled.");
            reportProgress(result.rootPath, true);
            return result;
        }

        iterator.next();
        ++result.visitedFileCount;

        const QFileInfo fileInfo = iterator.fileInfo();
        if (!isSupportedVideoFile(fileInfo.filePath())) {
            reportProgress(fileInfo.filePath());
            continue;
        }

        ScannedMediaFile file;
        file.fileName = fileInfo.fileName();
        file.filePath = normalizedPath(fileInfo);
        file.fileExtension = normalizedExtension(fileInfo);
        file.fileSize = fileInfo.size();
        file.modifiedAt = fileInfo.lastModified().toUTC().toString(Qt::ISODateWithMs);
        result.files.append(file);
        result.matchedFileCount = result.files.size();
        reportProgress(file.filePath);
    }

    result.succeeded = true;
    reportProgress(result.rootPath);
    return result;
}

bool ScanService::isSupportedVideoFile(const QString &filePath)
{
    static const QSet<QString> supportedExtensions = {
        QStringLiteral(".mp4"),
        QStringLiteral(".mkv"),
        QStringLiteral(".mov"),
        QStringLiteral(".avi"),
        QStringLiteral(".wmv"),
        QStringLiteral(".m4v"),
        QStringLiteral(".webm"),
        QStringLiteral(".mpg"),
        QStringLiteral(".mpeg"),
        QStringLiteral(".ts"),
    };

    return supportedExtensions.contains(normalizedExtension(QFileInfo(filePath)));
}

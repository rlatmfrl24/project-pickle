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

DirectoryScanResult ScanService::scanDirectory(const QString &rootPath) const
{
    DirectoryScanResult result;

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        result.errorMessage = QStringLiteral("Scan root is not a directory: %1").arg(rootPath);
        return result;
    }

    result.rootPath = normalizedPath(rootInfo);

    QDirIterator iterator(
        result.rootPath,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        iterator.next();

        const QFileInfo fileInfo = iterator.fileInfo();
        if (!isSupportedVideoFile(fileInfo.filePath())) {
            continue;
        }

        ScannedMediaFile file;
        file.fileName = fileInfo.fileName();
        file.filePath = normalizedPath(fileInfo);
        file.fileExtension = normalizedExtension(fileInfo);
        file.fileSize = fileInfo.size();
        file.modifiedAt = fileInfo.lastModified().toUTC().toString(Qt::ISODateWithMs);
        result.files.append(file);
    }

    result.succeeded = true;
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

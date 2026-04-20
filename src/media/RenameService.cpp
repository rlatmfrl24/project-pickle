#include "RenameService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

bool containsInvalidFileNameCharacter(const QString &baseName)
{
    static const QString invalidCharacters = QStringLiteral("<>:\"/\\|?*");
    for (const QChar character : baseName) {
        if (character.unicode() < 32 || invalidCharacters.contains(character)) {
            return true;
        }
    }

    return false;
}
}

FileRenameResult RenameService::renameFile(const QString &filePath, const QString &newBaseName) const
{
    FileRenameResult result;

    const QFileInfo sourceInfo(filePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        result.errorMessage = QStringLiteral("Original file was not found.");
        return result;
    }

    const QString trimmedBaseName = newBaseName.trimmed();
    if (trimmedBaseName.isEmpty()) {
        result.errorMessage = QStringLiteral("File name cannot be empty.");
        return result;
    }
    if (trimmedBaseName == QStringLiteral(".") || trimmedBaseName == QStringLiteral("..")) {
        result.errorMessage = QStringLiteral("File name is not valid.");
        return result;
    }
    if (trimmedBaseName.endsWith(QLatin1Char('.')) || trimmedBaseName.endsWith(QLatin1Char(' '))) {
        result.errorMessage = QStringLiteral("File name cannot end with a dot or space.");
        return result;
    }
    if (containsInvalidFileNameCharacter(trimmedBaseName)) {
        result.errorMessage = QStringLiteral("File name contains an invalid character.");
        return result;
    }

    const QString suffix = sourceInfo.suffix();
    const QString targetFileName = suffix.isEmpty()
        ? trimmedBaseName
        : QStringLiteral("%1.%2").arg(trimmedBaseName, suffix);
    const QDir parentDir = sourceInfo.dir();
    const QString targetPath = parentDir.absoluteFilePath(targetFileName);
    const QFileInfo targetInfo(targetPath);

    const QString sourcePath = normalizedPath(sourceInfo);
    const QString normalizedTargetPath = QDir::toNativeSeparators(targetInfo.absoluteFilePath());
    if (QString::compare(sourcePath, normalizedTargetPath, Qt::CaseInsensitive) != 0 && targetInfo.exists()) {
        result.errorMessage = QStringLiteral("A file with that name already exists.");
        return result;
    }

    if (sourcePath != normalizedTargetPath) {
        QFile sourceFile(sourceInfo.absoluteFilePath());
        if (!sourceFile.rename(targetInfo.absoluteFilePath())) {
            result.errorMessage = sourceFile.errorString();
            if (result.errorMessage.isEmpty()) {
                result.errorMessage = QStringLiteral("File rename failed.");
            }
            return result;
        }
    }

    const QFileInfo renamedInfo(targetInfo.absoluteFilePath());
    result.file.fileName = renamedInfo.fileName();
    result.file.filePath = normalizedPath(renamedInfo);
    result.file.fileExtension = normalizedExtension(renamedInfo);
    result.file.fileSize = renamedInfo.size();
    result.file.modifiedAt = renamedInfo.lastModified().toUTC().toString(Qt::ISODateWithMs);
    result.succeeded = true;
    return result;
}

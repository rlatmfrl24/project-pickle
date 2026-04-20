#include "PathSecurity.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace PathSecurity {

QString appDataPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.isEmpty()) {
        path = QDir::homePath() + QStringLiteral("/.pickle");
    }

    return QDir::toNativeSeparators(path);
}

QString pathForComparison(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    QString resolvedPath = info.canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = info.absoluteFilePath();
    }

    return QDir::cleanPath(QDir::toNativeSeparators(resolvedPath));
}

bool samePath(const QString &left, const QString &right)
{
    const QString normalizedLeft = pathForComparison(left);
    const QString normalizedRight = pathForComparison(right);
    return !normalizedLeft.isEmpty()
        && !normalizedRight.isEmpty()
        && QString::compare(normalizedLeft, normalizedRight, Qt::CaseInsensitive) == 0;
}

bool isInsideOrEqual(const QString &path, const QString &root)
{
    const QString normalizedPath = pathForComparison(path);
    const QString normalizedRoot = pathForComparison(root);
    if (normalizedPath.isEmpty() || normalizedRoot.isEmpty()) {
        return false;
    }

    return QString::compare(normalizedPath, normalizedRoot, Qt::CaseInsensitive) == 0
        || normalizedPath.startsWith(normalizedRoot + QLatin1Char('/'), Qt::CaseInsensitive)
        || normalizedPath.startsWith(normalizedRoot + QLatin1Char('\\'), Qt::CaseInsensitive);
}

QString redactedLocalPath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    const QString normalizedPath = QDir::toNativeSeparators(path);
    const QString homePath = QDir::toNativeSeparators(QDir::homePath());
    const QString dataPath = appDataPath();
    const QString fileName = QFileInfo(normalizedPath).fileName();
    const QString suffix = fileName.isEmpty() ? QString() : QStringLiteral("/%1").arg(fileName);

    if (!dataPath.isEmpty() && normalizedPath.startsWith(dataPath, Qt::CaseInsensitive)) {
        return QStringLiteral("<app-data>%1").arg(suffix);
    }
    if (!homePath.isEmpty() && normalizedPath.startsWith(homePath, Qt::CaseInsensitive)) {
        return QStringLiteral("<home>%1").arg(suffix);
    }

    return fileName.isEmpty() ? QStringLiteral("<path>") : QStringLiteral("<path>/%1").arg(fileName);
}

}

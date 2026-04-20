#include "AppLogger.h"

#include "core/PathSecurity.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QTextStream>

namespace {
constexpr qint64 MaxLogBytes = 1024 * 1024;

QMutex logMutex;
QFile logFile;
QtMessageHandler previousHandler = nullptr;

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("CRITICAL");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }

    return QStringLiteral("LOG");
}

void rotateLogIfNeeded(const QString &path)
{
    const QFileInfo currentLog(path);
    if (!currentLog.exists() || currentLog.size() <= MaxLogBytes) {
        return;
    }

    const QString rotatedPath = path + QStringLiteral(".1");
    QFile::remove(rotatedPath);
    QFile::rename(path, rotatedPath);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        QMutexLocker locker(&logMutex);
        if (logFile.isOpen()) {
            QTextStream stream(&logFile);
            stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                   << " [" << messageTypeName(type) << "] "
                   << message;
            if (context.file && context.line > 0) {
                stream << " (" << context.file << ":" << context.line << ")";
            }
            stream << Qt::endl;
            logFile.flush();
        }
    }

    if (previousHandler) {
        previousHandler(type, context, message);
    }
}
}

void AppLogger::initialize()
{
    const QString path = logPath();
    const QFileInfo logInfo(path);
    QDir().mkpath(logInfo.absolutePath());
    rotateLogIfNeeded(path);

    QMutexLocker locker(&logMutex);
    logFile.setFileName(path);
    logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    previousHandler = qInstallMessageHandler(messageHandler);
}

QString AppLogger::logPath()
{
    return QDir::toNativeSeparators(QDir(PathSecurity::appDataPath()).filePath(QStringLiteral("pickle.log")));
}

QString AppLogger::rotatedLogPath()
{
    return logPath() + QStringLiteral(".1");
}

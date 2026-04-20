#include "app/DiagnosticsController.h"

#include "core/AppLogger.h"
#include "core/PathSecurity.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QSysInfo>
#include <QTextStream>

QVariantMap DiagnosticsController::aboutInfo(const QString &databasePath) const
{
    return {
        {QStringLiteral("applicationName"), QCoreApplication::applicationName()},
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion().isEmpty()
                ? QStringLiteral("0.1")
                : QCoreApplication::applicationVersion()},
        {QStringLiteral("qtVersion"), QLibraryInfo::version().toString()},
        {QStringLiteral("databasePath"), databasePath},
    };
}

QVariantMap DiagnosticsController::systemInfo(const DiagnosticsContext &context) const
{
    return {
        {QStringLiteral("applicationName"), QCoreApplication::applicationName()},
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion().isEmpty()
                ? QStringLiteral("0.1")
                : QCoreApplication::applicationVersion()},
        {QStringLiteral("qtVersion"), QLibraryInfo::version().toString()},
        {QStringLiteral("os"), QSysInfo::prettyProductName()},
        {QStringLiteral("kernelType"), QSysInfo::kernelType()},
        {QStringLiteral("kernelVersion"), QSysInfo::kernelVersion()},
        {QStringLiteral("cpuArchitecture"), QSysInfo::currentCpuArchitecture()},
        {QStringLiteral("buildAbi"), QSysInfo::buildAbi()},
        {QStringLiteral("hostName"), QSysInfo::machineHostName()},
        {QStringLiteral("databaseReady"), context.databaseReady},
        {QStringLiteral("databaseStatus"), context.databaseStatus},
        {QStringLiteral("databasePath"), context.databasePath},
        {QStringLiteral("currentScanRoot"), context.currentScanRoot},
        {QStringLiteral("libraryItems"), context.libraryItems},
        {QStringLiteral("scanInProgress"), context.scanInProgress},
        {QStringLiteral("metadataInProgress"), context.metadataInProgress},
        {QStringLiteral("snapshotInProgress"), context.snapshotInProgress},
        {QStringLiteral("thumbnailMaintenanceInProgress"), context.thumbnailMaintenanceInProgress},
        {QStringLiteral("snapshotRoot"), SnapshotService::defaultSnapshotRoot()},
        {QStringLiteral("thumbnailRoot"), ThumbnailService::defaultThumbnailRoot()},
        {QStringLiteral("logPath"), AppLogger::logPath()},
        {QStringLiteral("rotatedLogPath"), AppLogger::rotatedLogPath()},
        {QStringLiteral("appDataPath"), PathSecurity::appDataPath()},
        {QStringLiteral("ffprobePath"), context.settings.ffprobePath},
        {QStringLiteral("ffmpegPath"), context.settings.ffmpegPath},
        {QStringLiteral("toolsStatus"), context.toolsStatus},
        {QStringLiteral("settingsStatus"), context.settingsStatus},
    };
}

QString DiagnosticsController::diagnosticReport(const DiagnosticsContext &context) const
{
    QString report;
    QTextStream stream(&report);
    const QVariantMap info = systemInfo(context);
    stream << "Pickle Diagnostic Report\n";
    stream << "Application: " << info.value(QStringLiteral("applicationName")).toString() << ' '
           << info.value(QStringLiteral("applicationVersion")).toString() << '\n';
    stream << "Qt: " << info.value(QStringLiteral("qtVersion")).toString() << '\n';
    stream << "OS: " << info.value(QStringLiteral("os")).toString() << '\n';
    stream << "Database: " << PathSecurity::redactedLocalPath(info.value(QStringLiteral("databasePath")).toString()) << '\n';
    stream << "Database status: " << info.value(QStringLiteral("databaseStatus")).toString() << '\n';
    stream << "App data: " << PathSecurity::redactedLocalPath(info.value(QStringLiteral("appDataPath")).toString()) << '\n';
    stream << "Log: " << PathSecurity::redactedLocalPath(info.value(QStringLiteral("logPath")).toString()) << '\n';
    stream << "Rotated log: " << PathSecurity::redactedLocalPath(info.value(QStringLiteral("rotatedLogPath")).toString()) << '\n';
    stream << "Snapshot root: " << PathSecurity::redactedLocalPath(info.value(QStringLiteral("snapshotRoot")).toString()) << '\n';
    stream << "Thumbnail root: " << PathSecurity::redactedLocalPath(info.value(QStringLiteral("thumbnailRoot")).toString()) << '\n';
    stream << "ffprobe path: " << (context.settings.ffprobePath.isEmpty() ? QStringLiteral("PATH") : PathSecurity::redactedLocalPath(context.settings.ffprobePath)) << '\n';
    stream << "ffmpeg path: " << (context.settings.ffmpegPath.isEmpty() ? QStringLiteral("PATH") : PathSecurity::redactedLocalPath(context.settings.ffmpegPath)) << '\n';
    stream << "Tools: " << context.toolsStatus << '\n';
    stream << "Settings: " << context.settingsStatus << '\n';
    stream << "Active work: scan=" << context.scanInProgress
           << ", metadata=" << context.metadataInProgress
           << ", snapshot=" << context.snapshotInProgress
           << ", thumbnail=" << context.thumbnailMaintenanceInProgress << '\n';
    stream << "\nRecent work events:\n";
    for (const QVariant &eventValue : context.workEvents) {
        const QVariantMap event = eventValue.toMap();
        stream << "- " << event.value(QStringLiteral("timestamp")).toString()
               << " [" << event.value(QStringLiteral("kind")).toString() << "] "
               << event.value(QStringLiteral("message")).toString() << '\n';
    }
    return report;
}

QString DiagnosticsController::openLogFile() const
{
    const QString path = AppLogger::logPath();
    if (!QFileInfo::exists(path)) {
        return QStringLiteral("Log file does not exist yet.");
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path))
        ? QStringLiteral("Log file opened")
        : QStringLiteral("Could not open log file.");
}

QString DiagnosticsController::openLogFolder() const
{
    const QString path = QFileInfo(AppLogger::logPath()).absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path))
        ? QStringLiteral("Log folder opened")
        : QStringLiteral("Could not open log folder.");
}

QString DiagnosticsController::openAppDataFolder() const
{
    const QString path = PathSecurity::appDataPath();
    QDir().mkpath(path);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path))
        ? QStringLiteral("App data folder opened")
        : QStringLiteral("Could not open app data folder.");
}

QString DiagnosticsController::localPathFromUrl(const QUrl &url) const
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) {
        path = url.toString(QUrl::PreferLocalFile);
    }
    return QDir::toNativeSeparators(path);
}

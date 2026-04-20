#include "app/SettingsController.h"

#include "application/SettingsUseCase.h"
#include "core/ExternalToolService.h"
#include "domain/AppSettingsPolicy.h"
#include "ports/IExternalToolResolver.h"
#include "ports/ISettingsRepository.h"

#include <QtGlobal>

namespace {
QString normalizedSortKeyName(const QString &sortKey)
{
    AppSettings settings;
    settings.sortKey = sortKey;
    return AppSettingsPolicy::normalized(settings).sortKey;
}
}

SettingsController::SettingsController(
    ISettingsRepository *settingsRepository,
    IExternalToolResolver *externalToolResolver)
    : m_settingsRepository(settingsRepository)
    , m_externalToolResolver(externalToolResolver)
{
    if (!m_externalToolResolver) {
        m_ownedExternalToolResolver = std::make_unique<ExternalToolService>();
        m_externalToolResolver = m_ownedExternalToolResolver.get();
    }

    if (m_settingsRepository) {
        SettingsUseCase useCase(m_settingsRepository);
        const OperationResult<AppSettings> result = useCase.load();
        if (result.succeeded) {
            m_settings = AppSettingsPolicy::normalized(result.value);
        } else if (!result.errorMessage.isEmpty()) {
            m_settingsStatus = QStringLiteral("Settings load failed: %1").arg(result.errorMessage);
        }
    }
    m_settings = AppSettingsPolicy::normalized(m_settings);
}

SettingsController::~SettingsController() = default;

AppSettings SettingsController::settings() const
{
    return m_settings;
}

QString SettingsController::settingsStatus() const
{
    return m_settingsStatus;
}

QString SettingsController::toolsStatus() const
{
    return m_toolsStatus;
}

ExternalToolStatus SettingsController::ffprobeStatus() const
{
    return m_ffprobeToolStatus;
}

ExternalToolStatus SettingsController::ffmpegStatus() const
{
    return m_ffmpegToolStatus;
}

SettingsUpdateResult SettingsController::saveSettings(const QVariantMap &settings)
{
    AppSettings nextSettings = m_settings;
    if (settings.contains(QStringLiteral("ffprobePath"))) {
        nextSettings.ffprobePath = settings.value(QStringLiteral("ffprobePath")).toString();
    }
    if (settings.contains(QStringLiteral("ffmpegPath"))) {
        nextSettings.ffmpegPath = settings.value(QStringLiteral("ffmpegPath")).toString();
    }
    if (settings.contains(QStringLiteral("showThumbnails"))) {
        nextSettings.showThumbnails = settings.value(QStringLiteral("showThumbnails")).toBool();
    }
    if (settings.contains(QStringLiteral("sortKey"))) {
        nextSettings.sortKey = settings.value(QStringLiteral("sortKey")).toString();
    }
    if (settings.contains(QStringLiteral("sortAscending"))) {
        nextSettings.sortAscending = settings.value(QStringLiteral("sortAscending")).toBool();
    }
    if (settings.contains(QStringLiteral("lastOpenFolder"))) {
        nextSettings.lastOpenFolder = settings.value(QStringLiteral("lastOpenFolder")).toString();
    }
    if (settings.contains(QStringLiteral("playerVolume"))) {
        nextSettings.playerVolume = settings.value(QStringLiteral("playerVolume")).toDouble();
    }
    if (settings.contains(QStringLiteral("playerMuted"))) {
        nextSettings.playerMuted = settings.value(QStringLiteral("playerMuted")).toBool();
    }

    return applySettings(nextSettings, QStringLiteral("Settings saved"));
}

SettingsUpdateResult SettingsController::setSortKey(const QString &sortKey)
{
    AppSettings nextSettings = m_settings;
    nextSettings.sortKey = normalizedSortKeyName(sortKey);
    return applySettings(nextSettings);
}

SettingsUpdateResult SettingsController::setSortAscending(bool ascending)
{
    AppSettings nextSettings = m_settings;
    nextSettings.sortAscending = ascending;
    return applySettings(nextSettings);
}

SettingsUpdateResult SettingsController::setShowThumbnails(bool showThumbnails)
{
    AppSettings nextSettings = m_settings;
    nextSettings.showThumbnails = showThumbnails;
    return applySettings(nextSettings);
}

SettingsUpdateResult SettingsController::setPlayerVolume(double volume)
{
    AppSettings nextSettings = m_settings;
    nextSettings.playerVolume = volume;
    return applySettings(nextSettings);
}

SettingsUpdateResult SettingsController::setPlayerMuted(bool muted)
{
    AppSettings nextSettings = m_settings;
    nextSettings.playerMuted = muted;
    return applySettings(nextSettings);
}

SettingsUpdateResult SettingsController::setLastOpenFolder(const QString &folderPath)
{
    AppSettings nextSettings = m_settings;
    nextSettings.lastOpenFolder = folderPath;
    return applySettings(nextSettings);
}

SettingsUpdateResult SettingsController::resetToolPathsToPath()
{
    AppSettings nextSettings = m_settings;
    nextSettings.ffprobePath.clear();
    nextSettings.ffmpegPath.clear();
    return applySettings(nextSettings, QStringLiteral("Tool paths reset to PATH"));
}

ToolValidationResult SettingsController::validateExternalTools()
{
    ToolValidationResult result;
    if (!m_externalToolResolver) {
        result.status = QStringLiteral("ffprobe unavailable, ffmpeg unavailable");
        m_toolsStatus = result.status;
        return result;
    }

    result.ffprobe = m_externalToolResolver->probeTool(QStringLiteral("ffprobe"), m_settings.ffprobePath);
    result.ffmpeg = m_externalToolResolver->probeTool(QStringLiteral("ffmpeg"), m_settings.ffmpegPath);
    result.allOk = result.ffprobe.succeeded && result.ffmpeg.succeeded;
    result.status = QStringLiteral("%1, %2").arg(
        result.ffprobe.succeeded ? QStringLiteral("ffprobe OK") : QStringLiteral("ffprobe unavailable"),
        result.ffmpeg.succeeded ? QStringLiteral("ffmpeg OK") : QStringLiteral("ffmpeg unavailable"));

    m_ffprobeToolStatus = result.ffprobe;
    m_ffmpegToolStatus = result.ffmpeg;
    m_toolsStatus = result.status;
    return result;
}

QString SettingsController::ffprobeProgram() const
{
    return m_externalToolResolver
        ? m_externalToolResolver->programForTool(QStringLiteral("ffprobe"), m_settings.ffprobePath)
        : QStringLiteral("ffprobe");
}

QString SettingsController::ffmpegProgram() const
{
    return m_externalToolResolver
        ? m_externalToolResolver->programForTool(QStringLiteral("ffmpeg"), m_settings.ffmpegPath)
        : QStringLiteral("ffmpeg");
}

void SettingsController::setSettingsStatus(const QString &status)
{
    m_settingsStatus = status;
}

void SettingsController::setToolsStatus(const QString &status)
{
    m_toolsStatus = status;
}

SettingsUpdateResult SettingsController::applySettings(AppSettings nextSettings, const QString &successStatus)
{
    nextSettings = AppSettingsPolicy::normalized(nextSettings);
    SettingsUpdateResult result;
    result.libraryQueryChanged = m_settings.sortKey != nextSettings.sortKey
        || m_settings.sortAscending != nextSettings.sortAscending;
    result.thumbnailVisibilityChanged = m_settings.showThumbnails != nextSettings.showThumbnails;
    result.settingsChanged = m_settings.ffprobePath != nextSettings.ffprobePath
        || m_settings.ffmpegPath != nextSettings.ffmpegPath
        || m_settings.showThumbnails != nextSettings.showThumbnails
        || m_settings.sortKey != nextSettings.sortKey
        || m_settings.sortAscending != nextSettings.sortAscending
        || m_settings.lastOpenFolder != nextSettings.lastOpenFolder
        || !qFuzzyCompare(m_settings.playerVolume + 1.0, nextSettings.playerVolume + 1.0)
        || m_settings.playerMuted != nextSettings.playerMuted;

    if (!result.settingsChanged) {
        return result;
    }

    m_settings = nextSettings;
    persistCurrentSettings(successStatus);
    result.status = m_settingsStatus;
    return result;
}

bool SettingsController::persistCurrentSettings(const QString &successStatus)
{
    m_settings = AppSettingsPolicy::normalized(m_settings);
    if (!m_settingsRepository) {
        if (!successStatus.isEmpty()) {
            m_settingsStatus = QStringLiteral("%1 (not persisted)").arg(successStatus);
        }
        return false;
    }

    SettingsUseCase useCase(m_settingsRepository);
    const VoidResult result = useCase.save(m_settings);
    if (!result.succeeded) {
        m_settingsStatus = QStringLiteral("Settings save failed: %1").arg(result.errorMessage);
        return false;
    }

    if (!successStatus.isEmpty()) {
        m_settingsStatus = successStatus;
    }
    return true;
}

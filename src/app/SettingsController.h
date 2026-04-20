#pragma once

#include "domain/MediaEntities.h"

#include <QString>
#include <QVariantMap>

#include <memory>

class IExternalToolResolver;
class ISettingsRepository;

struct SettingsUpdateResult
{
    bool settingsChanged = false;
    bool libraryQueryChanged = false;
    bool thumbnailVisibilityChanged = false;
    QString status;
};

struct ToolValidationResult
{
    ExternalToolStatus ffprobe;
    ExternalToolStatus ffmpeg;
    QString status;
    bool allOk = false;
};

class SettingsController
{
public:
    explicit SettingsController(
        ISettingsRepository *settingsRepository = nullptr,
        IExternalToolResolver *externalToolResolver = nullptr);
    ~SettingsController();

    AppSettings settings() const;
    QString settingsStatus() const;
    QString toolsStatus() const;
    ExternalToolStatus ffprobeStatus() const;
    ExternalToolStatus ffmpegStatus() const;

    SettingsUpdateResult saveSettings(const QVariantMap &settings);
    SettingsUpdateResult setSortKey(const QString &sortKey);
    SettingsUpdateResult setSortAscending(bool ascending);
    SettingsUpdateResult setShowThumbnails(bool showThumbnails);
    SettingsUpdateResult setPlayerVolume(double volume);
    SettingsUpdateResult setPlayerMuted(bool muted);
    SettingsUpdateResult setLastOpenFolder(const QString &folderPath);
    SettingsUpdateResult resetToolPathsToPath();

    ToolValidationResult validateExternalTools();
    QString ffprobeProgram() const;
    QString ffmpegProgram() const;
    void setSettingsStatus(const QString &status);
    void setToolsStatus(const QString &status);

private:
    SettingsUpdateResult applySettings(AppSettings nextSettings, const QString &successStatus = {});
    bool persistCurrentSettings(const QString &successStatus);

    ISettingsRepository *m_settingsRepository = nullptr;
    std::unique_ptr<IExternalToolResolver> m_ownedExternalToolResolver;
    IExternalToolResolver *m_externalToolResolver = nullptr;
    AppSettings m_settings;
    QString m_settingsStatus;
    QString m_toolsStatus = QStringLiteral("Tools not tested");
    ExternalToolStatus m_ffprobeToolStatus;
    ExternalToolStatus m_ffmpegToolStatus;
};

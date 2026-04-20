#pragma once

#include "media/MediaTypes.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class DiagnosticsController;
struct DiagnosticsContext;
class ISettingsRepository;
class IMediaRepository;
class LibraryController;
class MediaActionsController;
class MetadataController;
struct MetadataControllerResult;
class MediaLibraryModel;
class ScanController;
class SettingsController;
struct SettingsUpdateResult;
class SnapshotController;
class ThumbnailController;
struct ThumbnailMaintenanceResult;
class WorkEventLog;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedMedia READ selectedMedia NOTIFY selectedMediaChanged)
    Q_PROPERTY(bool databaseReady READ databaseReady NOTIFY databaseStateChanged)
    Q_PROPERTY(QString databaseStatus READ databaseStatus NOTIFY databaseStateChanged)
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY databaseStateChanged)
    Q_PROPERTY(bool scanInProgress READ scanInProgress NOTIFY scanStateChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStateChanged)
    Q_PROPERTY(QString currentScanRoot READ currentScanRoot NOTIFY scanStateChanged)
    Q_PROPERTY(int scanVisitedCount READ scanVisitedCount NOTIFY scanStateChanged)
    Q_PROPERTY(int scanFoundCount READ scanFoundCount NOTIFY scanStateChanged)
    Q_PROPERTY(QString scanProgressText READ scanProgressText NOTIFY scanStateChanged)
    Q_PROPERTY(bool scanCancelAvailable READ scanCancelAvailable NOTIFY scanStateChanged)
    Q_PROPERTY(QString librarySearchText READ librarySearchText WRITE setLibrarySearchText NOTIFY libraryStateChanged)
    Q_PROPERTY(QString librarySortKey READ librarySortKey WRITE setLibrarySortKey NOTIFY libraryStateChanged)
    Q_PROPERTY(bool librarySortAscending READ librarySortAscending WRITE setLibrarySortAscending NOTIFY libraryStateChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails WRITE setShowThumbnails NOTIFY libraryStateChanged)
    Q_PROPERTY(QString libraryStatus READ libraryStatus NOTIFY libraryStateChanged)
    Q_PROPERTY(bool metadataInProgress READ metadataInProgress NOTIFY metadataStateChanged)
    Q_PROPERTY(QString metadataStatus READ metadataStatus NOTIFY metadataStateChanged)
    Q_PROPERTY(QString detailStatus READ detailStatus NOTIFY detailStateChanged)
    Q_PROPERTY(QString fileActionStatus READ fileActionStatus NOTIFY fileActionStateChanged)
    Q_PROPERTY(bool snapshotInProgress READ snapshotInProgress NOTIFY snapshotStateChanged)
    Q_PROPERTY(QString snapshotStatus READ snapshotStatus NOTIFY snapshotStateChanged)
    Q_PROPERTY(QVariantList selectedSnapshots READ selectedSnapshots NOTIFY snapshotStateChanged)
    Q_PROPERTY(bool thumbnailMaintenanceInProgress READ thumbnailMaintenanceInProgress NOTIFY thumbnailMaintenanceStateChanged)
    Q_PROPERTY(QString thumbnailMaintenanceStatus READ thumbnailMaintenanceStatus NOTIFY thumbnailMaintenanceStateChanged)
    Q_PROPERTY(QString ffprobePath READ ffprobePath NOTIFY settingsStateChanged)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath NOTIFY settingsStateChanged)
    Q_PROPERTY(QString settingsStatus READ settingsStatus NOTIFY settingsStateChanged)
    Q_PROPERTY(QString toolsStatus READ toolsStatus NOTIFY settingsStateChanged)
    Q_PROPERTY(double playerVolume READ playerVolume WRITE setPlayerVolume NOTIFY settingsStateChanged)
    Q_PROPERTY(bool playerMuted READ playerMuted WRITE setPlayerMuted NOTIFY settingsStateChanged)
    Q_PROPERTY(QUrl lastOpenFolderUrl READ lastOpenFolderUrl NOTIFY settingsStateChanged)
    Q_PROPERTY(QVariantList workEvents READ workEvents NOTIFY workEventsChanged)

public:
    explicit AppController(
        MediaLibraryModel *mediaLibraryModel,
        IMediaRepository *mediaRepository,
        ISettingsRepository *settingsRepository = nullptr,
        QObject *parent = nullptr);
    ~AppController() override;

    int selectedIndex() const;
    void setSelectedIndex(int selectedIndex);

    QVariantMap selectedMedia() const;

    bool databaseReady() const;
    QString databaseStatus() const;
    QString databasePath() const;
    void setDatabaseState(bool ready, const QString &status, const QString &path);

    bool scanInProgress() const;
    QString scanStatus() const;
    QString currentScanRoot() const;
    int scanVisitedCount() const;
    int scanFoundCount() const;
    QString scanProgressText() const;
    bool scanCancelAvailable() const;

    QString librarySearchText() const;
    void setLibrarySearchText(const QString &searchText);
    QString librarySortKey() const;
    void setLibrarySortKey(const QString &sortKey);
    bool librarySortAscending() const;
    void setLibrarySortAscending(bool ascending);
    bool showThumbnails() const;
    void setShowThumbnails(bool showThumbnails);
    QString libraryStatus() const;
    bool metadataInProgress() const;
    QString metadataStatus() const;
    QString detailStatus() const;
    QString fileActionStatus() const;
    bool snapshotInProgress() const;
    QString snapshotStatus() const;
    QVariantList selectedSnapshots() const;
    bool thumbnailMaintenanceInProgress() const;
    QString thumbnailMaintenanceStatus() const;
    QString ffprobePath() const;
    QString ffmpegPath() const;
    QString settingsStatus() const;
    QString toolsStatus() const;
    double playerVolume() const;
    void setPlayerVolume(double volume);
    bool playerMuted() const;
    void setPlayerMuted(bool muted);
    QUrl lastOpenFolderUrl() const;
    QVariantList workEvents() const;

    Q_INVOKABLE void selectIndex(int index);
    Q_INVOKABLE void startDirectoryScan(const QUrl &folderUrl);
    Q_INVOKABLE void rescanCurrentRoot();
    Q_INVOKABLE void cancelScan();
    Q_INVOKABLE void refreshSelectedMetadata();
    Q_INVOKABLE void cancelMetadataRefresh();
    Q_INVOKABLE void saveSelectedMediaDetails(
        const QString &description,
        const QString &reviewStatus,
        int rating,
        const QVariantList &tags);
    Q_INVOKABLE void renameSelectedMedia(const QString &newBaseName);
    Q_INVOKABLE void setSelectedFavorite(bool enabled);
    Q_INVOKABLE void setSelectedDeleteCandidate(bool enabled);
    Q_INVOKABLE void saveSelectedPlaybackPosition(qint64 positionMs);
    Q_INVOKABLE void captureSelectedSnapshot(qint64 timestampMs);
    Q_INVOKABLE void cancelSnapshotCapture();
    Q_INVOKABLE void cancelActiveWork();
    Q_INVOKABLE void rebuildThumbnailCache();
    Q_INVOKABLE void cancelThumbnailMaintenance();
    Q_INVOKABLE void resetLibrary();
    Q_INVOKABLE QVariantMap aboutInfo() const;
    Q_INVOKABLE QVariantMap systemInfo() const;
    Q_INVOKABLE void saveSettings(const QVariantMap &settings);
    Q_INVOKABLE void resetToolPathsToPath();
    Q_INVOKABLE void validateExternalTools();
    Q_INVOKABLE void openLogFile();
    Q_INVOKABLE void openLogFolder();
    Q_INVOKABLE void openAppDataFolder();
    Q_INVOKABLE QString diagnosticReport() const;
    Q_INVOKABLE void clearWorkEvents();
    Q_INVOKABLE QString localPathFromUrl(const QUrl &url) const;

signals:
    void selectedIndexChanged();
    void selectedMediaChanged();
    void databaseStateChanged();
    void scanStateChanged();
    void libraryStateChanged();
    void metadataStateChanged();
    void detailStateChanged();
    void fileActionStateChanged();
    void snapshotStateChanged();
    void thumbnailMaintenanceStateChanged();
    void settingsStateChanged();
    void workEventsChanged();

private:
    void startDirectoryScanPath(const QString &rootPath);
    void handleScanFinished(const ScanCommitResult &commitResult);
    void handleLibraryReloadFinished(const LibraryLoadResult &result);
    void handleMetadataFinished(const MetadataControllerResult &result);
    void handleSnapshotFinished(const SnapshotCaptureResult &result);
    void handleThumbnailMaintenanceFinished(const ThumbnailMaintenanceResult &result);
    void maybeStartAutoMetadataExtraction();
    void runAutoMetadataExtraction();
    void startMetadataExtraction(const QVariantMap &media, bool manual);
    void refreshSelectedSnapshots();
    bool hasActiveBackgroundWork() const;
    void requestLibraryReload(int delayMs);
    bool applyLibraryItems(QVector<MediaLibraryItem> items);
    void syncSelectionAfterLibraryChange();
    void setScanState(bool inProgress, const QString &status, const QString &currentScanRoot);
    void setScanProgress(int visitedCount, int foundCount, const QString &progressText);
    void setLibraryStatus(const QString &status);
    void setMetadataState(bool inProgress, const QString &status);
    void setDetailStatus(const QString &status);
    void setFileActionStatus(const QString &status);
    void setSnapshotState(bool inProgress, const QString &status);
    void setThumbnailMaintenanceState(bool inProgress, const QString &status);
    void setSettingsStatus(const QString &status);
    void setToolsStatus(const QString &status);
    QString ffprobeProgram() const;
    QString ffmpegProgram() const;
    void recordWorkEvent(const QString &kind, const QString &message, bool warning = false);
    MediaLibraryQuery libraryQuery() const;
    void emitSettingsUpdate(const SettingsUpdateResult &result);
    DiagnosticsContext diagnosticsContext() const;

    MediaLibraryModel *m_mediaLibraryModel = nullptr;
    IMediaRepository *m_mediaRepository = nullptr;
    std::unique_ptr<SettingsController> m_settingsController;
    std::unique_ptr<DiagnosticsController> m_diagnosticsController;
    std::unique_ptr<MediaActionsController> m_mediaActionsController;
    std::unique_ptr<WorkEventLog> m_workEventLog;
    std::unique_ptr<ScanController> m_scanController;
    std::unique_ptr<LibraryController> m_libraryController;
    std::unique_ptr<MetadataController> m_metadataController;
    std::unique_ptr<SnapshotController> m_snapshotController;
    std::unique_ptr<ThumbnailController> m_thumbnailController;
    int m_selectedIndex = 0;
    int m_selectedMediaId = -1;
    bool m_databaseReady = false;
    QString m_databaseStatus = QStringLiteral("DB not initialized");
    QString m_databasePath;
    bool m_scanInProgress = false;
    QString m_scanStatus = QStringLiteral("No scan yet");
    QString m_currentScanRoot;
    int m_scanVisitedCount = 0;
    int m_scanFoundCount = 0;
    QString m_scanProgressText;
    QString m_librarySearchText;
    QString m_librarySortKey = QStringLiteral("name");
    bool m_librarySortAscending = true;
    bool m_showThumbnails = true;
    QString m_libraryStatus = QStringLiteral("Library not loaded");
    bool m_metadataInProgress = false;
    bool m_autoMetadataQueued = false;
    QString m_metadataStatus;
    QString m_detailStatus;
    QString m_fileActionStatus;
    bool m_snapshotInProgress = false;
    QString m_snapshotStatus;
    QVariantList m_selectedSnapshots;
    bool m_thumbnailMaintenanceInProgress = false;
    QString m_thumbnailMaintenanceStatus;
    QList<int> m_autoMetadataFailures;
};

#include "AppController.h"

#include "app/DiagnosticsController.h"
#include "app/LibraryController.h"
#include "app/MediaActionsController.h"
#include "app/MetadataController.h"
#include "app/ScanController.h"
#include "app/SettingsController.h"
#include "app/SnapshotController.h"
#include "app/ThumbnailController.h"
#include "app/WorkEventLog.h"
#include "media/MediaLibraryModel.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"
#include "ports/IMediaRepository.h"

#include <QMetaObject>
#include <QFileInfo>

#include <algorithm>
#include <memory>
#include <utility>

namespace {
MediaLibrarySortKey sortKeyFromString(const QString &sortKey)
{
    const QString normalized = sortKey.trimmed().toLower();
    if (normalized == QStringLiteral("size")) {
        return MediaLibrarySortKey::Size;
    }
    if (normalized == QStringLiteral("modified")) {
        return MediaLibrarySortKey::Modified;
    }
    if (normalized == QStringLiteral("lastplayed")) {
        return MediaLibrarySortKey::LastPlayed;
    }
    return MediaLibrarySortKey::Name;
}

QString normalizedViewModeName(const QString &viewMode)
{
    const QString normalized = viewMode.trimmed().toLower();
    if (normalized == QStringLiteral("unreviewed")) return QStringLiteral("unreviewed");
    if (normalized == QStringLiteral("favorites")) return QStringLiteral("favorites");
    if (normalized == QStringLiteral("deletecandidates")) return QStringLiteral("deleteCandidates");
    if (normalized == QStringLiteral("recent")) return QStringLiteral("recent");
    return QStringLiteral("all");
}

MediaLibraryViewMode viewModeFromString(const QString &viewMode)
{
    const QString normalized = normalizedViewModeName(viewMode);
    if (normalized == QStringLiteral("unreviewed")) return MediaLibraryViewMode::Unreviewed;
    if (normalized == QStringLiteral("favorites")) return MediaLibraryViewMode::Favorites;
    if (normalized == QStringLiteral("deleteCandidates")) return MediaLibraryViewMode::DeleteCandidates;
    if (normalized == QStringLiteral("recent")) return MediaLibraryViewMode::Recent;
    return MediaLibraryViewMode::All;
}

bool mediaNeedsMetadata(const QVariantMap &media)
{
    return !media.isEmpty()
        && media.value(QStringLiteral("durationMs")).toLongLong() <= 0
        && (media.value(QStringLiteral("resolution")).toString().isEmpty()
            || media.value(QStringLiteral("resolution")).toString() == QStringLiteral("-"))
        && (media.value(QStringLiteral("codec")).toString().isEmpty()
            || media.value(QStringLiteral("codec")).toString() == QStringLiteral("-"));
}

bool statusLooksLikeWarning(const QString &status)
{
    const QString text = status.toLower();
    return text.contains(QStringLiteral("failed"))
        || text.contains(QStringLiteral("error"))
        || text.contains(QStringLiteral("could not"))
        || text.contains(QStringLiteral("unavailable"));
}

bool containsMediaId(const std::vector<int> &mediaIds, int mediaId)
{
    return std::find(mediaIds.cbegin(), mediaIds.cend(), mediaId) != mediaIds.cend();
}

void appendMediaIdOnce(std::vector<int> *mediaIds, int mediaId)
{
    if (mediaId > 0 && mediaIds && !containsMediaId(*mediaIds, mediaId)) {
        mediaIds->push_back(mediaId);
    }
}

void removeMediaId(std::vector<int> *mediaIds, int mediaId)
{
    if (!mediaIds) {
        return;
    }
    mediaIds->erase(std::remove(mediaIds->begin(), mediaIds->end(), mediaId), mediaIds->end());
}

}

AppController::AppController(
    MediaLibraryModel *mediaLibraryModel,
    IMediaRepository *mediaRepository,
    ISettingsRepository *settingsRepository,
    QObject *parent)
    : QObject(parent)
    , m_mediaLibraryModel(mediaLibraryModel)
    , m_mediaRepository(mediaRepository)
    , m_settingsController(std::make_unique<SettingsController>(settingsRepository))
    , m_diagnosticsController(std::make_unique<DiagnosticsController>())
    , m_mediaActionsController(std::make_unique<MediaActionsController>(mediaRepository, mediaLibraryModel))
    , m_workEventLog(std::make_unique<WorkEventLog>())
    , m_scanController(std::make_unique<ScanController>())
    , m_libraryController(std::make_unique<LibraryController>())
    , m_metadataController(std::make_unique<MetadataController>())
    , m_snapshotController(std::make_unique<SnapshotController>())
    , m_thumbnailController(std::make_unique<ThumbnailController>())
{
    const AppSettings settings = m_settingsController->settings();
    m_librarySortKey = settings.sortKey;
    m_librarySortAscending = settings.sortAscending;
    m_showThumbnails = settings.showThumbnails;
    m_selectedIndex = (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) ? -1 : 0;
    m_selectedMediaId = m_selectedIndex >= 0 ? m_mediaLibraryModel->mediaIdAt(m_selectedIndex) : -1;
    if (m_selectedMediaId > 0) {
        m_selectedMediaIds.append(m_selectedMediaId);
        m_selectionAnchorMediaId = m_selectedMediaId;
        m_mediaLibraryModel->setSelectedMediaIds(m_selectedMediaIds);
    }
    m_libraryStatus = QStringLiteral("Showing %1 item(s)").arg(m_mediaLibraryModel ? m_mediaLibraryModel->rowCount() : 0);

    connect(m_scanController.get(), &ScanController::scanRejected, this, [this](const QString &status, const QString &root) { setScanState(false, status, root); });
    connect(m_scanController.get(), &ScanController::scanStarted, this, [this](const QString &status, const QString &root) { setScanState(true, status, root); });
    connect(m_scanController.get(), &ScanController::progressChanged, this, &AppController::setScanProgress);
    connect(m_scanController.get(), &ScanController::scanFinished, this, &AppController::handleScanFinished);
    connect(m_libraryController.get(), &LibraryController::statusChanged, this, &AppController::setLibraryStatus);
    connect(m_libraryController.get(), &LibraryController::loadFinished, this, &AppController::handleLibraryReloadFinished);
    connect(m_metadataController.get(), &MetadataController::extractionRejected, this, [this](const QString &status) { if (!m_shuttingDown) setMetadataState(false, status); }, Qt::QueuedConnection);
    connect(m_metadataController.get(), &MetadataController::extractionStarted, this, [this](const QString &status) { if (!m_shuttingDown) setMetadataState(true, status); }, Qt::QueuedConnection);
    connect(m_metadataController.get(), &MetadataController::extractionFinished, this, &AppController::handleMetadataFinished, Qt::QueuedConnection);
    connect(m_snapshotController.get(), &SnapshotController::captureRejected, this, [this](const QString &status) { setSnapshotState(false, status); });
    connect(m_snapshotController.get(), &SnapshotController::captureStarted, this, [this](const QString &status) { setSnapshotState(true, status); });
    connect(m_snapshotController.get(), &SnapshotController::captureFinished, this, &AppController::handleSnapshotFinished);
    connect(m_thumbnailController.get(), &ThumbnailController::rebuildRejected, this, [this](const QString &status) { setThumbnailMaintenanceState(false, status); });
    connect(m_thumbnailController.get(), &ThumbnailController::rebuildStarted, this, [this](const QString &status) { setThumbnailMaintenanceState(true, status); });
    connect(m_thumbnailController.get(), &ThumbnailController::rebuildProgressed, this, [this](const QString &status) { setThumbnailMaintenanceState(true, status); });
    connect(m_thumbnailController.get(), &ThumbnailController::rebuildFinished, this, &AppController::handleThumbnailMaintenanceFinished);
}

AppController::~AppController()
{
    m_shuttingDown = true;
    m_autoMetadataQueued = false;
    if (m_scanController) disconnect(m_scanController.get(), nullptr, this, nullptr); if (m_libraryController) disconnect(m_libraryController.get(), nullptr, this, nullptr); if (m_metadataController) disconnect(m_metadataController.get(), nullptr, this, nullptr); if (m_snapshotController) disconnect(m_snapshotController.get(), nullptr, this, nullptr); if (m_thumbnailController) disconnect(m_thumbnailController.get(), nullptr, this, nullptr);
    cancelActiveWork();
    if (m_scanController) m_scanController->waitForFinished();
    if (m_libraryController) { m_libraryController->cancelPending(); m_libraryController->waitForFinished(); }
    if (m_metadataController) m_metadataController->waitForFinished();
    if (m_snapshotController) m_snapshotController->waitForFinished();
    if (m_thumbnailController) m_thumbnailController->waitForFinished();
}

int AppController::selectedIndex() const { return m_selectedIndex; }
QVariantMap AppController::selectedMedia() const { return (!m_mediaLibraryModel || m_selectedIndex < 0) ? QVariantMap {} : m_mediaLibraryModel->get(m_selectedIndex); }
bool AppController::databaseReady() const { return m_databaseReady; }
QString AppController::databaseStatus() const { return m_databaseStatus; }
QString AppController::databasePath() const { return m_databasePath; }
bool AppController::scanInProgress() const { return m_scanInProgress; }
QString AppController::scanStatus() const { return m_scanStatus; }
QString AppController::currentScanRoot() const { return m_currentScanRoot; }
int AppController::scanVisitedCount() const { return m_scanVisitedCount; }
int AppController::scanFoundCount() const { return m_scanFoundCount; }
QString AppController::scanProgressText() const { return m_scanProgressText; }
bool AppController::scanCancelAvailable() const { return m_scanInProgress && m_scanController && m_scanController->cancelAvailable(); }
QString AppController::librarySearchText() const { return m_librarySearchText; }
QString AppController::libraryViewMode() const { return m_libraryViewMode; }
QString AppController::libraryTagFilter() const { return m_libraryTagFilter; }
QStringList AppController::availableTags() const { return m_availableTags; }
QString AppController::librarySortKey() const { return m_librarySortKey; }
bool AppController::librarySortAscending() const { return m_librarySortAscending; }
bool AppController::showThumbnails() const { return m_showThumbnails; }
QString AppController::libraryStatus() const { return m_libraryStatus; }
bool AppController::metadataInProgress() const { return m_metadataInProgress; }
QString AppController::metadataStatus() const { return m_metadataStatus; }
QString AppController::detailStatus() const { return m_detailStatus; }
QString AppController::fileActionStatus() const { return m_fileActionStatus; }
bool AppController::snapshotInProgress() const { return m_snapshotInProgress; }
QString AppController::snapshotStatus() const { return m_snapshotStatus; }
QVariantList AppController::selectedSnapshots() const { return m_selectedSnapshots; }
bool AppController::thumbnailMaintenanceInProgress() const { return m_thumbnailMaintenanceInProgress; }
QString AppController::thumbnailMaintenanceStatus() const { return m_thumbnailMaintenanceStatus; }
QString AppController::ffprobePath() const { return m_settingsController->settings().ffprobePath; }
QString AppController::ffmpegPath() const { return m_settingsController->settings().ffmpegPath; }
QString AppController::settingsStatus() const { return m_settingsController->settingsStatus(); }
QString AppController::toolsStatus() const { return m_settingsController->toolsStatus(); }
double AppController::playerVolume() const { return m_settingsController->settings().playerVolume; }
bool AppController::playerMuted() const { return m_settingsController->settings().playerMuted; }
QUrl AppController::lastOpenFolderUrl() const { const QString path = m_settingsController->settings().lastOpenFolder; return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path); }
QVariantList AppController::workEvents() const { return m_workEventLog->events(); }

void AppController::setDatabaseState(bool ready, const QString &status, const QString &path)
{
    if (m_databaseReady == ready && m_databaseStatus == status && m_databasePath == path) return;
    m_databaseReady = ready;
    m_databaseStatus = status;
    m_databasePath = path;
    emit databaseStateChanged();
    refreshAvailableTags();
    refreshSelectedSnapshots();
    maybeStartAutoMetadataExtraction();
}

void AppController::setLibrarySearchText(const QString &searchText) { if (m_librarySearchText == searchText) return; m_librarySearchText = searchText; emit libraryStateChanged(); requestLibraryReload(150); }
void AppController::setLibraryViewMode(const QString &viewMode) { const QString normalized = normalizedViewModeName(viewMode); if (m_libraryViewMode == normalized) return; m_libraryViewMode = normalized; if (normalized == QStringLiteral("recent")) { m_librarySortKey = QStringLiteral("lastPlayed"); m_librarySortAscending = false; } emit libraryStateChanged(); requestLibraryReload(0); }
void AppController::setLibraryTagFilter(const QString &tagFilter) { const QString normalized = tagFilter.trimmed(); if (m_libraryTagFilter == normalized) return; m_libraryTagFilter = normalized; emit libraryStateChanged(); requestLibraryReload(0); }
void AppController::setLibrarySortKey(const QString &sortKey) { emitSettingsUpdate(m_settingsController->setSortKey(sortKey)); }
void AppController::setLibrarySortAscending(bool ascending) { emitSettingsUpdate(m_settingsController->setSortAscending(ascending)); }
void AppController::setShowThumbnails(bool showThumbnails) { emitSettingsUpdate(m_settingsController->setShowThumbnails(showThumbnails)); }
void AppController::setPlayerVolume(double volume) { emitSettingsUpdate(m_settingsController->setPlayerVolume(volume)); }
void AppController::setPlayerMuted(bool muted) { emitSettingsUpdate(m_settingsController->setPlayerMuted(muted)); }
void AppController::startDirectoryScan(const QUrl &folderUrl) { startDirectoryScanPath(m_diagnosticsController->localPathFromUrl(folderUrl)); }
void AppController::rescanCurrentRoot() { startDirectoryScanPath(m_currentScanRoot); }
void AppController::refreshSelectedMetadata() { startMetadataExtraction(selectedMedia(), true); }
void AppController::saveSelectedMediaDetails(const QString &description, const QString &reviewStatus, int rating, const QVariantList &tags) { const MediaActionResult r = m_mediaActionsController->saveDetails(selectedMedia(), description, reviewStatus, rating, tags, m_databaseReady, hasActiveBackgroundWork()); if (r.selectedMediaChanged) emit selectedMediaChanged(); if (r.libraryReloadRequested) { refreshAvailableTags(); requestLibraryReload(0); } setDetailStatus(r.status); }
void AppController::renameSelectedMedia(const QString &newBaseName) { const MediaActionResult r = m_mediaActionsController->renameMedia(selectedMedia(), newBaseName, m_databaseReady, hasActiveBackgroundWork()); if (r.selectedMediaChanged) emit selectedMediaChanged(); if (r.libraryReloadRequested) requestLibraryReload(0); setFileActionStatus(r.status); }
void AppController::setSelectedFavorite(bool enabled) { const MediaActionResult r = m_mediaActionsController->setFavorite(selectedMedia(), enabled, m_databaseReady, hasActiveBackgroundWork()); if (r.selectedMediaChanged) { emit selectedMediaChanged(); if (m_libraryViewMode == QStringLiteral("favorites")) requestLibraryReload(0); } setFileActionStatus(r.status); }
void AppController::setSelectedDeleteCandidate(bool enabled) { const MediaActionResult r = m_mediaActionsController->setDeleteCandidate(selectedMedia(), enabled, m_databaseReady, hasActiveBackgroundWork()); if (r.selectedMediaChanged) { emit selectedMediaChanged(); if (m_libraryViewMode == QStringLiteral("deleteCandidates")) requestLibraryReload(0); } setFileActionStatus(r.status); }
void AppController::saveSelectedPlaybackPosition(qint64 positionMs) { const MediaActionResult r = m_mediaActionsController->savePlaybackPosition(selectedMedia(), positionMs, m_databaseReady); if (r.selectedMediaChanged) { emit selectedMediaChanged(); if (m_libraryViewMode == QStringLiteral("recent")) requestLibraryReload(0); } if (!r.status.isEmpty()) setFileActionStatus(r.status); else if (m_fileActionStatus.startsWith(QStringLiteral("Playback position save failed"))) setFileActionStatus({}); }
void AppController::cancelScan() { if (m_scanInProgress && m_scanController && m_scanController->cancel()) setScanState(true, QStringLiteral("Canceling scan..."), m_currentScanRoot); }
void AppController::cancelMetadataRefresh() { if (m_metadataInProgress && m_metadataController && m_metadataController->cancel()) setMetadataState(true, QStringLiteral("Canceling metadata...")); }
void AppController::cancelSnapshotCapture() { if (m_snapshotInProgress && m_snapshotController && m_snapshotController->cancel()) setSnapshotState(true, QStringLiteral("Canceling snapshot...")); }
void AppController::cancelThumbnailMaintenance() { if (m_thumbnailMaintenanceInProgress && m_thumbnailController && m_thumbnailController->cancel()) setThumbnailMaintenanceState(true, QStringLiteral("Canceling thumbnail rebuild...")); }
void AppController::cancelActiveWork() { cancelScan(); cancelMetadataRefresh(); cancelSnapshotCapture(); cancelThumbnailMaintenance(); }

void AppController::captureSelectedSnapshot(qint64 timestampMs)
{
    const QVariantMap media = selectedMedia();
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    if (mediaId <= 0 || filePath.isEmpty()) { setSnapshotState(false, QStringLiteral("Select a media item before capturing a snapshot.")); return; }
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) { setSnapshotState(false, QStringLiteral("Database is not ready for snapshots.")); return; }
    if (m_scanInProgress || m_metadataInProgress || m_thumbnailMaintenanceInProgress) { setSnapshotState(false, QStringLiteral("Wait for active work to finish before capturing a snapshot.")); return; }
    if (m_snapshotInProgress || (m_snapshotController && m_snapshotController->isRunning())) { setSnapshotState(true, QStringLiteral("Snapshot capture already in progress")); return; }
    SnapshotCaptureRequest request;
    request.mediaId = mediaId;
    request.filePath = filePath;
    request.ffmpegProgram = ffmpegProgram();
    request.outputRootPath = SnapshotService::defaultSnapshotRoot();
    request.thumbnailRootPath = ThumbnailService::defaultThumbnailRoot();
    request.thumbnailMaxWidth = 320;
    request.timestampMs = std::max<qint64>(0, timestampMs);
    if (m_snapshotController) m_snapshotController->startCapture(request, media.value(QStringLiteral("fileName")).toString());
}

void AppController::rebuildThumbnailCache()
{
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) { setThumbnailMaintenanceState(false, QStringLiteral("Database is not ready for thumbnail rebuild.")); return; }
    if (hasActiveBackgroundWork()) { setThumbnailMaintenanceState(m_thumbnailMaintenanceInProgress, QStringLiteral("Wait for active work to finish before rebuilding thumbnails.")); return; }
    const QVector<ThumbnailBackfillItem> items = m_mediaRepository->fetchThumbnailBackfillItems();
    if (!m_mediaRepository->lastError().isEmpty()) { setThumbnailMaintenanceState(false, QStringLiteral("Thumbnail rebuild failed: %1").arg(m_mediaRepository->lastError())); return; }
    if (m_thumbnailController) m_thumbnailController->startRebuild(items, ThumbnailService::defaultThumbnailRoot(), 320, 82);
}

void AppController::resetLibrary()
{
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) { setLibraryStatus(QStringLiteral("Database is not ready for library reset.")); return; }
    if (hasActiveBackgroundWork()) { setLibraryStatus(QStringLiteral("Wait for active work to finish before resetting the library.")); return; }
    const LibraryResetResult result = m_mediaActionsController->resetLibrary();
    if (!result.succeeded) { setLibraryStatus(result.libraryStatus); return; }
    m_selectedIndex = -1; m_selectedMediaId = -1; m_selectedMediaIds.clear(); m_selectionAnchorMediaId = -1; m_autoMetadataQueued = false; m_currentScanRoot.clear(); m_autoMetadataFailures.clear(); m_autoMetadataAttempted.clear();
    m_selectedSnapshots.clear(); m_availableTags.clear(); m_libraryTagFilter.clear(); m_scanVisitedCount = 0; m_scanFoundCount = 0; m_scanProgressText.clear();
    if (m_libraryController) m_libraryController->cancelPending();
    if (m_scanController) m_scanController->clearCurrentRoot();
    m_mediaLibraryModel->setSelectedMediaIds({});
    emit selectedIndexChanged(); emit selectedMediaChanged(); emit selectionStateChanged();
    setScanState(false, QStringLiteral("No scan root"), {});
    setMetadataState(false, {});
    setSnapshotState(false, result.snapshotStatus);
    setThumbnailMaintenanceState(false, result.thumbnailStatus);
    setDetailStatus({});
    setFileActionStatus(result.fileActionStatus);
    setLibraryStatus(result.libraryStatus);
}

QVariantMap AppController::aboutInfo() const { return m_diagnosticsController->aboutInfo(m_databasePath); }
QVariantMap AppController::systemInfo() const { return m_diagnosticsController->systemInfo(diagnosticsContext()); }
void AppController::saveSettings(const QVariantMap &settings) { emitSettingsUpdate(m_settingsController->saveSettings(settings)); }
void AppController::resetToolPathsToPath() { emitSettingsUpdate(m_settingsController->resetToolPathsToPath()); }
void AppController::validateExternalTools() { const ToolValidationResult r = m_settingsController->validateExternalTools(); emit settingsStateChanged(); recordWorkEvent(QStringLiteral("tools"), r.status, !r.allOk); }
void AppController::openLogFile() { setSettingsStatus(m_diagnosticsController->openLogFile()); }
void AppController::openLogFolder() { setSettingsStatus(m_diagnosticsController->openLogFolder()); }
void AppController::openAppDataFolder() { setSettingsStatus(m_diagnosticsController->openAppDataFolder()); }
QString AppController::diagnosticReport() const { return m_diagnosticsController->diagnosticReport(diagnosticsContext()); }
void AppController::clearWorkEvents() { if (m_workEventLog->clear()) emit workEventsChanged(); }
QString AppController::localPathFromUrl(const QUrl &url) const { return m_diagnosticsController->localPathFromUrl(url); }

void AppController::startDirectoryScanPath(const QString &rootPath)
{
    if (m_scanInProgress) { setScanState(true, QStringLiteral("Scan already in progress"), m_currentScanRoot); return; }
    if (hasActiveBackgroundWork()) { setScanState(false, QStringLiteral("Wait for active work to finish before scanning"), m_currentScanRoot); return; }
    if (!m_databaseReady || !m_mediaRepository) { setScanState(false, QStringLiteral("Database is not ready for scanning"), m_currentScanRoot); return; }
    if (m_databasePath.trimmed().isEmpty()) { setScanState(false, QStringLiteral("Database path is not available for scanning"), m_currentScanRoot); return; }
    if (m_scanController) m_scanController->startScan(rootPath, m_databasePath);
}

void AppController::handleScanFinished(const ScanCommitResult &commitResult)
{
    const DirectoryScanResult scanResult = commitResult.scan;
    const QString scanRoot = scanResult.rootPath.isEmpty() ? m_currentScanRoot : scanResult.rootPath;
    setScanProgress(scanResult.visitedFileCount, scanResult.matchedFileCount, m_scanProgressText);
    if (scanResult.canceled) { setScanState(false, QStringLiteral("Scan canceled"), scanRoot); setScanProgress(scanResult.visitedFileCount, scanResult.matchedFileCount, QStringLiteral("Scan canceled")); return; }
    if (!scanResult.succeeded) { setScanState(false, QStringLiteral("Scan failed: %1").arg(scanResult.errorMessage), scanRoot); return; }
    if (!commitResult.succeeded) { setScanState(false, QStringLiteral("DB update failed: %1").arg(commitResult.errorMessage), scanRoot); return; }
    requestLibraryReload(0);
    setScanState(false, QStringLiteral("Scan complete: %1 video file(s)").arg(commitResult.upsert.upsertedFileCount), scanRoot);
    setScanProgress(scanResult.visitedFileCount, scanResult.matchedFileCount, QStringLiteral("Scan complete"));
    if (!scanRoot.isEmpty() && m_settingsController->settings().lastOpenFolder != scanRoot) emitSettingsUpdate(m_settingsController->setLastOpenFolder(scanRoot));
}

void AppController::handleMetadataFinished(const MetadataControllerResult &controllerResult)
{
    if (m_shuttingDown) return;
    const MetadataExtractionResult result = controllerResult.extraction;
    if (result.canceled) { setMetadataState(false, QStringLiteral("Metadata canceled")); maybeStartAutoMetadataExtraction(); return; }
    appendMediaIdOnce(&m_autoMetadataAttempted, controllerResult.mediaId);
    if (!result.succeeded) { if (!controllerResult.manual) appendMediaIdOnce(&m_autoMetadataFailures, controllerResult.mediaId); setMetadataState(false, QStringLiteral("Metadata failed: %1").arg(result.errorMessage)); maybeStartAutoMetadataExtraction(); return; }
    if (!m_mediaRepository || !m_mediaRepository->updateMediaMetadata(controllerResult.mediaId, result.metadata)) { setMetadataState(false, QStringLiteral("Metadata DB update failed: %1").arg(m_mediaRepository ? m_mediaRepository->lastError() : QStringLiteral("repository unavailable"))); maybeStartAutoMetadataExtraction(); return; }
    const MediaActionResult refresh = m_mediaActionsController->refreshMediaFromRepository(controllerResult.mediaId);
    if (refresh.selectedMediaChanged && controllerResult.mediaId == m_selectedMediaId) emit selectedMediaChanged();
    requestLibraryReload(0);
    setMetadataState(false, QStringLiteral("Metadata updated"));
    maybeStartAutoMetadataExtraction();
}

void AppController::handleSnapshotFinished(const SnapshotCaptureResult &result)
{
    const MediaActionResult action = m_mediaActionsController->applySnapshotResult(result);
    if (action.selectedMediaChanged && result.mediaId == m_selectedMediaId) { refreshSelectedSnapshots(); emit selectedMediaChanged(); }
    setSnapshotState(false, action.status);
}

void AppController::handleThumbnailMaintenanceFinished(const ThumbnailMaintenanceResult &result)
{
    const MediaActionResult action = m_mediaActionsController->applyThumbnailMaintenanceResult(result.generated, result.canceled);
    if (action.selectedMediaChanged) emit selectedMediaChanged();
    setThumbnailMaintenanceState(false, action.status);
}

void AppController::maybeStartAutoMetadataExtraction()
{
    if (m_shuttingDown || m_autoMetadataQueued || !m_databaseReady || !m_mediaRepository) return;
    m_autoMetadataQueued = true;
    QMetaObject::invokeMethod(this, [this]() {
        if (m_shuttingDown) return;
        m_autoMetadataQueued = false;
        runAutoMetadataExtraction();
    }, Qt::QueuedConnection);
}

void AppController::runAutoMetadataExtraction()
{
    if (!m_databaseReady || !m_mediaRepository || m_metadataInProgress || m_scanInProgress || m_snapshotInProgress || m_thumbnailMaintenanceInProgress || (m_metadataController && m_metadataController->isRunning())) return;
    const QVariantMap media = selectedMedia();
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    if (filePath.isEmpty() || !QFileInfo(filePath).isFile()) return;
    if (mediaId <= 0 || containsMediaId(m_autoMetadataAttempted, mediaId) || containsMediaId(m_autoMetadataFailures, mediaId) || !mediaNeedsMetadata(media)) return;
    appendMediaIdOnce(&m_autoMetadataAttempted, mediaId);
    startMetadataExtraction(media, false);
}

void AppController::startMetadataExtraction(const QVariantMap &media, bool manual)
{
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    if (mediaId <= 0 || filePath.isEmpty()) { setMetadataState(false, QStringLiteral("Select a media item before refreshing metadata.")); return; }
    if (!m_databaseReady || !m_mediaRepository) { setMetadataState(false, QStringLiteral("Database is not ready for metadata updates.")); return; }
    if (m_scanInProgress || m_snapshotInProgress || m_thumbnailMaintenanceInProgress) { setMetadataState(false, QStringLiteral("Wait for active work to finish before reading metadata.")); return; }
    if (m_metadataInProgress || (m_metadataController && m_metadataController->isRunning())) { setMetadataState(true, QStringLiteral("Metadata refresh already in progress")); return; }
    if (manual) { removeMediaId(&m_autoMetadataFailures, mediaId); removeMediaId(&m_autoMetadataAttempted, mediaId); }
    if (m_metadataController) m_metadataController->startExtraction(mediaId, filePath, media.value(QStringLiteral("fileName")).toString(), manual, ffprobeProgram());
}

void AppController::refreshSelectedSnapshots()
{
    const QVariantList nextSnapshots = m_mediaActionsController->snapshotsForMedia(m_selectedMediaId, m_databaseReady);
    if (m_selectedSnapshots == nextSnapshots) return;
    m_selectedSnapshots = nextSnapshots;
    emit snapshotStateChanged();
}

void AppController::refreshAvailableTags()
{
    const QStringList nextTags = (m_databaseReady && m_mediaRepository) ? m_mediaRepository->fetchTagNames() : QStringList {};
    if (m_availableTags == nextTags) return;
    m_availableTags = nextTags;
    emit libraryStateChanged();
}

bool AppController::hasActiveBackgroundWork() const
{
    return m_scanInProgress || m_metadataInProgress || m_snapshotInProgress || m_thumbnailMaintenanceInProgress
        || (m_scanController && m_scanController->isRunning())
        || (m_libraryController && m_libraryController->isRunning())
        || (m_metadataController && m_metadataController->isRunning())
        || (m_snapshotController && m_snapshotController->isRunning())
        || (m_thumbnailController && m_thumbnailController->isRunning());
}

void AppController::requestLibraryReload(int delayMs) { if (m_databaseReady && !m_databasePath.trimmed().isEmpty() && m_libraryController) m_libraryController->requestReload(m_databasePath, libraryQuery(), delayMs); }
void AppController::handleLibraryReloadFinished(const LibraryLoadResult &result) { if (!result.succeeded) { setLibraryStatus(QStringLiteral("Library refresh failed: %1").arg(result.errorMessage)); return; } const int preserved = m_selectedMediaId; applyLibraryItems(QVector<MediaLibraryItem>(result.items)); m_selectedMediaId = preserved; syncSelectionAfterLibraryChange(); }
bool AppController::applyLibraryItems(QVector<MediaLibraryItem> items) { if (!m_mediaLibraryModel) return false; m_mediaLibraryModel->setItems(std::move(items)); setLibraryStatus(QStringLiteral("Showing %1 item(s)").arg(m_mediaLibraryModel->rowCount())); return true; }

void AppController::syncSelectionAfterLibraryChange()
{
    QVector<int> nextIds = visibleMediaIds(m_selectedMediaIds);
    int primaryId = m_selectedMediaId;
    if (nextIds.isEmpty() && m_mediaLibraryModel && m_mediaLibraryModel->rowCount() > 0) {
        const int preservedIndex = primaryId > 0 ? m_mediaLibraryModel->indexOfId(primaryId) : -1;
        primaryId = m_mediaLibraryModel->mediaIdAt(preservedIndex >= 0 ? preservedIndex : 0);
        nextIds.append(primaryId);
    }
    applySelection(nextIds, primaryId, m_selectionAnchorMediaId);
}

void AppController::setScanState(bool inProgress, const QString &status, const QString &root) { if (m_scanInProgress == inProgress && m_scanStatus == status && m_currentScanRoot == root) return; m_scanInProgress = inProgress; m_scanStatus = status; m_currentScanRoot = root; emit scanStateChanged(); if (!status.isEmpty()) recordWorkEvent(QStringLiteral("scan"), status, statusLooksLikeWarning(status)); }
void AppController::setScanProgress(int visited, int found, const QString &text) { if (m_scanVisitedCount == visited && m_scanFoundCount == found && m_scanProgressText == text) return; m_scanVisitedCount = visited; m_scanFoundCount = found; m_scanProgressText = text; emit scanStateChanged(); }
void AppController::setLibraryStatus(const QString &status) { if (m_libraryStatus == status) return; m_libraryStatus = status; emit libraryStateChanged(); }
void AppController::setMetadataState(bool inProgress, const QString &status) { if (m_metadataInProgress == inProgress && m_metadataStatus == status) return; m_metadataInProgress = inProgress; m_metadataStatus = status; emit metadataStateChanged(); if (!status.isEmpty()) recordWorkEvent(QStringLiteral("metadata"), status, statusLooksLikeWarning(status)); }
void AppController::setDetailStatus(const QString &status) { if (m_detailStatus == status) return; m_detailStatus = status; emit detailStateChanged(); }
void AppController::setFileActionStatus(const QString &status) { if (m_fileActionStatus == status) return; m_fileActionStatus = status; emit fileActionStateChanged(); }
void AppController::setSnapshotState(bool inProgress, const QString &status) { if (m_snapshotInProgress == inProgress && m_snapshotStatus == status) return; m_snapshotInProgress = inProgress; m_snapshotStatus = status; emit snapshotStateChanged(); if (!status.isEmpty()) recordWorkEvent(QStringLiteral("snapshot"), status, statusLooksLikeWarning(status)); }
void AppController::setThumbnailMaintenanceState(bool inProgress, const QString &status) { if (m_thumbnailMaintenanceInProgress == inProgress && m_thumbnailMaintenanceStatus == status) return; m_thumbnailMaintenanceInProgress = inProgress; m_thumbnailMaintenanceStatus = status; emit thumbnailMaintenanceStateChanged(); if (!status.isEmpty()) recordWorkEvent(QStringLiteral("thumbnail"), status, statusLooksLikeWarning(status)); }
void AppController::setSettingsStatus(const QString &status) { if (m_settingsController->settingsStatus() == status) return; m_settingsController->setSettingsStatus(status); emit settingsStateChanged(); if (!status.isEmpty()) recordWorkEvent(QStringLiteral("settings"), status, statusLooksLikeWarning(status)); }
void AppController::setToolsStatus(const QString &status) { if (m_settingsController->toolsStatus() == status) return; m_settingsController->setToolsStatus(status); emit settingsStateChanged(); }
QString AppController::ffprobeProgram() const { return m_settingsController->ffprobeProgram(); }
QString AppController::ffmpegProgram() const { return m_settingsController->ffmpegProgram(); }
void AppController::recordWorkEvent(const QString &kind, const QString &message, bool warning) { if (m_workEventLog->append(kind, message, warning)) emit workEventsChanged(); }
MediaLibraryQuery AppController::libraryQuery() const { MediaLibraryQuery query; query.searchText = m_librarySearchText; query.tagFilter = m_libraryTagFilter; query.viewMode = viewModeFromString(m_libraryViewMode); query.sortKey = sortKeyFromString(m_librarySortKey); query.ascending = m_librarySortAscending; return query; }

void AppController::emitSettingsUpdate(const SettingsUpdateResult &result)
{
    if (!result.settingsChanged) return;
    const AppSettings settings = m_settingsController->settings();
    m_librarySortKey = settings.sortKey;
    m_librarySortAscending = settings.sortAscending;
    m_showThumbnails = settings.showThumbnails;
    emit settingsStateChanged();
    if (result.libraryQueryChanged || result.thumbnailVisibilityChanged) emit libraryStateChanged();
    if (result.libraryQueryChanged) requestLibraryReload(0);
    if (!result.status.isEmpty()) recordWorkEvent(QStringLiteral("settings"), result.status, statusLooksLikeWarning(result.status));
}

DiagnosticsContext AppController::diagnosticsContext() const
{
    DiagnosticsContext context;
    context.databaseReady = m_databaseReady;
    context.databaseStatus = m_databaseStatus;
    context.databasePath = m_databasePath;
    context.currentScanRoot = m_currentScanRoot;
    context.libraryItems = m_mediaLibraryModel ? m_mediaLibraryModel->rowCount() : 0;
    context.scanInProgress = m_scanInProgress;
    context.metadataInProgress = m_metadataInProgress;
    context.snapshotInProgress = m_snapshotInProgress;
    context.thumbnailMaintenanceInProgress = m_thumbnailMaintenanceInProgress;
    context.settings = m_settingsController->settings();
    context.toolsStatus = m_settingsController->toolsStatus();
    context.settingsStatus = m_settingsController->settingsStatus();
    context.workEvents = m_workEventLog->events();
    return context;
}

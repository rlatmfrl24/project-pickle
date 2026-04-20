#include "AppController.h"

#include "application/CaptureSnapshotUseCase.h"
#include "application/ExtractMetadataUseCase.h"
#include "application/RenameMediaUseCase.h"
#include "application/SavePlaybackPositionUseCase.h"
#include "application/UpdateMediaDetailsUseCase.h"
#include "application/UpdateMediaFlagsUseCase.h"
#include "app/LibraryController.h"
#include "app/ScanController.h"
#include "core/AppLogger.h"
#include "core/CancellationToken.h"
#include "core/ExternalToolService.h"
#include "core/PathSecurity.h"
#include "db/AppSettingsRepository.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "media/MetadataService.h"
#include "media/RenameService.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QPointer>
#include <QSysInfo>
#include <QTextStream>
#include <QtConcurrent>
#include <QDebug>

#include <algorithm>
#include <memory>
#include <utility>

namespace {
MediaLibrarySortKey sortKeyFromString(const QString &sortKey)
{
    const QString normalizedSortKey = sortKey.trimmed().toLower();
    if (normalizedSortKey == QStringLiteral("size")) {
        return MediaLibrarySortKey::Size;
    }
    if (normalizedSortKey == QStringLiteral("modified")) {
        return MediaLibrarySortKey::Modified;
    }

    return MediaLibrarySortKey::Name;
}

QString normalizedSortKeyName(const QString &sortKey)
{
    const QString normalizedSortKey = sortKey.trimmed().toLower();
    if (normalizedSortKey == QStringLiteral("size") || normalizedSortKey == QStringLiteral("modified")) {
        return normalizedSortKey;
    }

    return QStringLiteral("name");
}

bool mediaNeedsMetadata(const QVariantMap &media)
{
    if (media.isEmpty()) {
        return false;
    }

    const qint64 durationMs = media.value(QStringLiteral("durationMs")).toLongLong();
    const QString resolution = media.value(QStringLiteral("resolution")).toString();
    const QString codec = media.value(QStringLiteral("codec")).toString();
    return durationMs <= 0
        && (resolution.isEmpty() || resolution == QStringLiteral("-"))
        && (codec.isEmpty() || codec == QStringLiteral("-"));
}

QStringList stringListFromVariantList(const QVariantList &values)
{
    QStringList result;
    for (const QVariant &value : values) {
        result.append(value.toString());
    }

    return result;
}

QVariantMap snapshotToMap(const SnapshotItem &snapshot)
{
    return {
        {QStringLiteral("id"), snapshot.id},
        {QStringLiteral("mediaId"), snapshot.mediaId},
        {QStringLiteral("imagePath"), snapshot.imagePath},
        {QStringLiteral("timestampMs"), snapshot.timestampMs},
        {QStringLiteral("createdAt"), snapshot.createdAt},
    };
}

bool statusLooksLikeWarning(const QString &status)
{
    const QString normalizedStatus = status.toLower();
    return normalizedStatus.contains(QStringLiteral("failed"))
        || normalizedStatus.contains(QStringLiteral("error"))
        || normalizedStatus.contains(QStringLiteral("could not"))
        || normalizedStatus.contains(QStringLiteral("unavailable"));
}

}

AppController::AppController(
    MediaLibraryModel *mediaLibraryModel,
    MediaRepository *mediaRepository,
    AppSettingsRepository *settingsRepository,
    QObject *parent)
    : QObject(parent)
    , m_mediaLibraryModel(mediaLibraryModel)
    , m_mediaRepository(mediaRepository)
    , m_settingsRepository(settingsRepository)
{
    if (m_settingsRepository) {
        m_appSettings = m_settingsRepository->load();
        if (!m_settingsRepository->lastError().isEmpty()) {
            m_settingsStatus = QStringLiteral("Settings load failed: %1").arg(m_settingsRepository->lastError());
        }
    }
    m_appSettings = AppSettingsRepository::normalized(m_appSettings);
    m_librarySortKey = m_appSettings.sortKey;
    m_librarySortAscending = m_appSettings.sortAscending;
    m_showThumbnails = m_appSettings.showThumbnails;

    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) {
        m_selectedIndex = -1;
        m_selectedMediaId = -1;
    } else {
        m_selectedMediaId = m_mediaLibraryModel->mediaIdAt(m_selectedIndex);
    }
    m_libraryStatus = QStringLiteral("Showing %1 item(s)").arg(m_mediaLibraryModel ? m_mediaLibraryModel->rowCount() : 0);

    m_scanController = std::make_unique<ScanController>();
    m_libraryController = std::make_unique<LibraryController>();
    m_metadataWatcher = std::make_unique<QFutureWatcher<void>>();
    m_snapshotWatcher = std::make_unique<QFutureWatcher<void>>();
    m_thumbnailMaintenanceWatcher = std::make_unique<QFutureWatcher<void>>();

    connect(m_scanController.get(), &ScanController::scanRejected, this, [this](const QString &status, const QString &currentRoot) {
        setScanState(false, status, currentRoot);
    });
    connect(m_scanController.get(), &ScanController::scanStarted, this, [this](const QString &status, const QString &currentRoot) {
        setScanState(true, status, currentRoot);
    });
    connect(m_scanController.get(), &ScanController::progressChanged, this, &AppController::setScanProgress);
    connect(m_scanController.get(), &ScanController::scanFinished, this, &AppController::handleScanFinished);
    connect(m_libraryController.get(), &LibraryController::statusChanged, this, &AppController::setLibraryStatus);
    connect(m_libraryController.get(), &LibraryController::loadFinished, this, &AppController::handleLibraryReloadFinished);
    connect(m_metadataWatcher.get(), &QFutureWatcher<void>::finished, this, &AppController::handleMetadataFinished);
    connect(m_snapshotWatcher.get(), &QFutureWatcher<void>::finished, this, &AppController::handleSnapshotFinished);
    connect(m_thumbnailMaintenanceWatcher.get(), &QFutureWatcher<void>::finished, this, &AppController::handleThumbnailMaintenanceFinished);
}

AppController::~AppController()
{
    disconnect(m_scanController.get(), nullptr, this, nullptr);
    disconnect(m_libraryController.get(), nullptr, this, nullptr);
    disconnect(m_metadataWatcher.get(), nullptr, this, nullptr);
    disconnect(m_snapshotWatcher.get(), nullptr, this, nullptr);
    disconnect(m_thumbnailMaintenanceWatcher.get(), nullptr, this, nullptr);
    cancelActiveWork();
    if (m_scanController) {
        m_scanController->waitForFinished();
    }
    if (m_libraryController) {
        m_libraryController->cancelPending();
        m_libraryController->waitForFinished();
    }
    if (m_metadataWatcher && m_metadataWatcher->isRunning()) {
        m_metadataWatcher->waitForFinished();
    }
    if (m_snapshotWatcher && m_snapshotWatcher->isRunning()) {
        m_snapshotWatcher->waitForFinished();
    }
    if (m_thumbnailMaintenanceWatcher && m_thumbnailMaintenanceWatcher->isRunning()) {
        m_thumbnailMaintenanceWatcher->waitForFinished();
    }
    m_metadataResult.reset();
    m_snapshotResult.reset();
    m_thumbnailMaintenanceResult.reset();
}

int AppController::selectedIndex() const
{
    return m_selectedIndex;
}

void AppController::setSelectedIndex(int selectedIndex)
{
    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) {
        selectedIndex = -1;
    } else if (selectedIndex < 0 || selectedIndex >= m_mediaLibraryModel->rowCount()) {
        return;
    }

    if (m_selectedIndex == selectedIndex) {
        return;
    }

    m_selectedIndex = selectedIndex;
    m_selectedMediaId = selectedIndex >= 0 ? m_mediaLibraryModel->mediaIdAt(selectedIndex) : -1;
    emit selectedIndexChanged();
    emit selectedMediaChanged();
    refreshSelectedSnapshots();
    maybeStartAutoMetadataExtraction();
}

QVariantMap AppController::selectedMedia() const
{
    if (!m_mediaLibraryModel || m_selectedIndex < 0) {
        return {};
    }

    return m_mediaLibraryModel->get(m_selectedIndex);
}

bool AppController::databaseReady() const
{
    return m_databaseReady;
}

QString AppController::databaseStatus() const
{
    return m_databaseStatus;
}

QString AppController::databasePath() const
{
    return m_databasePath;
}

void AppController::setDatabaseState(bool ready, const QString &status, const QString &path)
{
    if (m_databaseReady == ready && m_databaseStatus == status && m_databasePath == path) {
        return;
    }

    m_databaseReady = ready;
    m_databaseStatus = status;
    m_databasePath = path;
    emit databaseStateChanged();
    refreshSelectedSnapshots();
    maybeStartAutoMetadataExtraction();
}

void AppController::selectIndex(int index)
{
    setSelectedIndex(index);
}

bool AppController::scanInProgress() const
{
    return m_scanInProgress;
}

QString AppController::scanStatus() const
{
    return m_scanStatus;
}

QString AppController::currentScanRoot() const
{
    return m_currentScanRoot;
}

int AppController::scanVisitedCount() const
{
    return m_scanVisitedCount;
}

int AppController::scanFoundCount() const
{
    return m_scanFoundCount;
}

QString AppController::scanProgressText() const
{
    return m_scanProgressText;
}

bool AppController::scanCancelAvailable() const
{
    return m_scanInProgress
        && m_scanController
        && m_scanController->cancelAvailable();
}

QString AppController::librarySearchText() const
{
    return m_librarySearchText;
}

void AppController::setLibrarySearchText(const QString &searchText)
{
    if (m_librarySearchText == searchText) {
        return;
    }

    m_librarySearchText = searchText;
    emit libraryStateChanged();

    requestLibraryReload(150);
}

QString AppController::librarySortKey() const
{
    return m_librarySortKey;
}

void AppController::setLibrarySortKey(const QString &sortKey)
{
    const QString normalizedSortKey = normalizedSortKeyName(sortKey);
    if (m_librarySortKey == normalizedSortKey) {
        return;
    }

    m_librarySortKey = normalizedSortKey;
    m_appSettings.sortKey = normalizedSortKey;
    emit libraryStateChanged();
    saveCurrentSettings();

    requestLibraryReload(0);
}

bool AppController::librarySortAscending() const
{
    return m_librarySortAscending;
}

void AppController::setLibrarySortAscending(bool ascending)
{
    if (m_librarySortAscending == ascending) {
        return;
    }

    m_librarySortAscending = ascending;
    m_appSettings.sortAscending = ascending;
    emit libraryStateChanged();
    saveCurrentSettings();

    requestLibraryReload(0);
}

bool AppController::showThumbnails() const
{
    return m_showThumbnails;
}

void AppController::setShowThumbnails(bool showThumbnails)
{
    if (m_showThumbnails == showThumbnails) {
        return;
    }

    m_showThumbnails = showThumbnails;
    m_appSettings.showThumbnails = showThumbnails;
    emit libraryStateChanged();
    saveCurrentSettings();
}

QString AppController::libraryStatus() const
{
    return m_libraryStatus;
}

bool AppController::metadataInProgress() const
{
    return m_metadataInProgress;
}

QString AppController::metadataStatus() const
{
    return m_metadataStatus;
}

QString AppController::detailStatus() const
{
    return m_detailStatus;
}

QString AppController::fileActionStatus() const
{
    return m_fileActionStatus;
}

bool AppController::snapshotInProgress() const
{
    return m_snapshotInProgress;
}

QString AppController::snapshotStatus() const
{
    return m_snapshotStatus;
}

QVariantList AppController::selectedSnapshots() const
{
    return m_selectedSnapshots;
}

bool AppController::thumbnailMaintenanceInProgress() const
{
    return m_thumbnailMaintenanceInProgress;
}

QString AppController::thumbnailMaintenanceStatus() const
{
    return m_thumbnailMaintenanceStatus;
}

QString AppController::ffprobePath() const
{
    return m_appSettings.ffprobePath;
}

QString AppController::ffmpegPath() const
{
    return m_appSettings.ffmpegPath;
}

QString AppController::settingsStatus() const
{
    return m_settingsStatus;
}

QString AppController::toolsStatus() const
{
    return m_toolsStatus;
}

double AppController::playerVolume() const
{
    return m_appSettings.playerVolume;
}

void AppController::setPlayerVolume(double volume)
{
    AppSettings nextSettings = m_appSettings;
    nextSettings.playerVolume = volume;
    nextSettings = AppSettingsRepository::normalized(nextSettings);
    if (qFuzzyCompare(m_appSettings.playerVolume + 1.0, nextSettings.playerVolume + 1.0)) {
        return;
    }

    m_appSettings.playerVolume = nextSettings.playerVolume;
    emit settingsStateChanged();
    saveCurrentSettings();
}

bool AppController::playerMuted() const
{
    return m_appSettings.playerMuted;
}

void AppController::setPlayerMuted(bool muted)
{
    if (m_appSettings.playerMuted == muted) {
        return;
    }

    m_appSettings.playerMuted = muted;
    emit settingsStateChanged();
    saveCurrentSettings();
}

QUrl AppController::lastOpenFolderUrl() const
{
    return m_appSettings.lastOpenFolder.isEmpty()
        ? QUrl()
        : QUrl::fromLocalFile(m_appSettings.lastOpenFolder);
}

QVariantList AppController::workEvents() const
{
    return m_workEvents;
}

void AppController::startDirectoryScan(const QUrl &folderUrl)
{
    QString rootPath = folderUrl.toLocalFile();
    if (rootPath.isEmpty()) {
        rootPath = folderUrl.toString(QUrl::PreferLocalFile);
    }

    startDirectoryScanPath(rootPath);
}

void AppController::rescanCurrentRoot()
{
    startDirectoryScanPath(m_currentScanRoot);
}

void AppController::cancelScan()
{
    if (!m_scanInProgress || !m_scanController || !m_scanController->cancel()) {
        return;
    }

    setScanState(true, QStringLiteral("Canceling scan..."), m_currentScanRoot);
}

void AppController::refreshSelectedMetadata()
{
    startMetadataExtraction(selectedMedia(), true);
}

void AppController::cancelMetadataRefresh()
{
    if (!m_metadataInProgress || !m_metadataCancellation) {
        return;
    }

    m_metadataCancellation->cancel();
    setMetadataState(true, QStringLiteral("Canceling metadata..."));
}

void AppController::saveSelectedMediaDetails(
    const QString &description,
    const QString &reviewStatus,
    int rating,
    const QVariantList &tags)
{
    const QVariantMap media = selectedMedia();
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    if (mediaId <= 0) {
        setDetailStatus(QStringLiteral("Select a media item before saving details."));
        return;
    }

    if (!m_databaseReady || !m_mediaRepository) {
        setDetailStatus(QStringLiteral("Database is not ready for saving details."));
        return;
    }

    if (hasActiveBackgroundWork()) {
        setDetailStatus(QStringLiteral("Wait for active work to finish before saving details."));
        return;
    }

    const QStringList tagNames = stringListFromVariantList(tags);
    UpdateMediaDetailsUseCase useCase(m_mediaRepository);
    const VoidResult result = useCase.execute(mediaId, description, reviewStatus, rating, tagNames);
    if (!result.succeeded) {
        setDetailStatus(QStringLiteral("Detail save failed: %1").arg(result.errorMessage));
        return;
    }

    if (m_mediaLibraryModel) {
        const int row = m_mediaLibraryModel->indexOfId(mediaId);
        if (row >= 0) {
            MediaLibraryItem item = m_mediaLibraryModel->itemAt(row);
            item.description = description;
            item.reviewStatus = reviewStatus;
            item.rating = rating;
            item.tags = tagNames;
            if (m_mediaLibraryModel->replaceItem(mediaId, item)) {
                emit selectedMediaChanged();
            }
        }
    }
    requestLibraryReload(0);
    setDetailStatus(QStringLiteral("Details saved"));
}

void AppController::renameSelectedMedia(const QString &newBaseName)
{
    const QVariantMap media = selectedMedia();
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    if (mediaId <= 0 || filePath.isEmpty()) {
        setFileActionStatus(QStringLiteral("Select a media item before renaming."));
        return;
    }
    if (!m_databaseReady || !m_mediaRepository) {
        setFileActionStatus(QStringLiteral("Database is not ready for rename."));
        return;
    }
    if (hasActiveBackgroundWork()) {
        setFileActionStatus(QStringLiteral("Wait for active work to finish before renaming."));
        return;
    }

    const QFileInfo originalInfo(filePath);
    RenameService renameService;
    RenameMediaUseCase useCase(&renameService, m_mediaRepository);
    const OperationResult<ScannedMediaFile> renameResult = useCase.execute(
        mediaId,
        filePath,
        newBaseName,
        originalInfo.completeBaseName());
    if (!renameResult.succeeded) {
        setFileActionStatus(QStringLiteral("Rename failed: %1").arg(renameResult.errorMessage));
        return;
    }

    if (m_mediaLibraryModel) {
        const int row = m_mediaLibraryModel->indexOfId(mediaId);
        if (row >= 0) {
            MediaLibraryItem item = m_mediaLibraryModel->itemAt(row);
            item.fileName = renameResult.value.fileName;
            item.filePath = renameResult.value.filePath;
            item.fileSizeBytes = renameResult.value.fileSize;
            item.modifiedAt = renameResult.value.modifiedAt;
            if (m_mediaLibraryModel->replaceItem(mediaId, item)) {
                emit selectedMediaChanged();
            }
        }
    }
    requestLibraryReload(0);
    setFileActionStatus(QStringLiteral("Renamed to %1").arg(renameResult.value.fileName));
}

void AppController::setSelectedFavorite(bool enabled)
{
    const int mediaId = selectedMedia().value(QStringLiteral("id")).toInt();
    if (mediaId <= 0) {
        setFileActionStatus(QStringLiteral("Select a media item before changing favorite."));
        return;
    }
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) {
        setFileActionStatus(QStringLiteral("Database is not ready for favorite updates."));
        return;
    }
    if (hasActiveBackgroundWork()) {
        setFileActionStatus(QStringLiteral("Wait for active work to finish before updating favorite."));
        return;
    }
    UpdateMediaFlagsUseCase useCase(m_mediaRepository);
    const VoidResult result = useCase.execute(mediaId, MediaFlagKind::Favorite, enabled);
    if (!result.succeeded) {
        setFileActionStatus(QStringLiteral("Favorite update failed: %1").arg(result.errorMessage));
        return;
    }
    m_mediaLibraryModel->setFavorite(mediaId, enabled);
    emit selectedMediaChanged();

    setFileActionStatus(enabled ? QStringLiteral("Marked as favorite") : QStringLiteral("Favorite removed"));
}

void AppController::setSelectedDeleteCandidate(bool enabled)
{
    const int mediaId = selectedMedia().value(QStringLiteral("id")).toInt();
    if (mediaId <= 0) {
        setFileActionStatus(QStringLiteral("Select a media item before changing delete candidate."));
        return;
    }
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) {
        setFileActionStatus(QStringLiteral("Database is not ready for delete candidate updates."));
        return;
    }
    if (hasActiveBackgroundWork()) {
        setFileActionStatus(QStringLiteral("Wait for active work to finish before updating delete candidate."));
        return;
    }
    UpdateMediaFlagsUseCase useCase(m_mediaRepository);
    const VoidResult result = useCase.execute(mediaId, MediaFlagKind::DeleteCandidate, enabled);
    if (!result.succeeded) {
        setFileActionStatus(QStringLiteral("Delete candidate update failed: %1").arg(result.errorMessage));
        return;
    }
    m_mediaLibraryModel->setDeleteCandidate(mediaId, enabled);
    emit selectedMediaChanged();

    setFileActionStatus(enabled ? QStringLiteral("Marked as delete candidate") : QStringLiteral("Delete candidate removed"));
}

void AppController::saveSelectedPlaybackPosition(qint64 positionMs)
{
    const int mediaId = selectedMedia().value(QStringLiteral("id")).toInt();
    if (mediaId <= 0 || !m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) {
        return;
    }
    const qint64 normalizedPositionMs = std::max<qint64>(0, positionMs);
    const qint64 currentPositionMs = selectedMedia().value(QStringLiteral("lastPositionMs")).toLongLong();
    const qint64 positionDelta = currentPositionMs > normalizedPositionMs
        ? currentPositionMs - normalizedPositionMs
        : normalizedPositionMs - currentPositionMs;
    if (positionDelta < 1000) {
        return;
    }

    const QString playedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    SavePlaybackPositionUseCase useCase(m_mediaRepository);
    const VoidResult result = useCase.execute(mediaId, normalizedPositionMs, playedAt);
    if (!result.succeeded) {
        setFileActionStatus(QStringLiteral("Playback position save failed: %1").arg(result.errorMessage));
        return;
    }
    m_mediaLibraryModel->setPlaybackPosition(mediaId, normalizedPositionMs, playedAt);
    emit selectedMediaChanged();

    if (!m_fileActionStatus.isEmpty()
        && m_fileActionStatus.startsWith(QStringLiteral("Playback position save failed"))) {
        setFileActionStatus({});
    }
}

void AppController::captureSelectedSnapshot(qint64 timestampMs)
{
    const QVariantMap media = selectedMedia();
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    const QString fileName = media.value(QStringLiteral("fileName")).toString();

    if (mediaId <= 0 || filePath.isEmpty()) {
        setSnapshotState(false, QStringLiteral("Select a media item before capturing a snapshot."));
        return;
    }
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) {
        setSnapshotState(false, QStringLiteral("Database is not ready for snapshots."));
        return;
    }
    if (m_scanInProgress || m_metadataInProgress || m_thumbnailMaintenanceInProgress) {
        setSnapshotState(false, QStringLiteral("Wait for active work to finish before capturing a snapshot."));
        return;
    }
    if (m_snapshotInProgress || (m_snapshotWatcher && m_snapshotWatcher->isRunning())) {
        setSnapshotState(true, QStringLiteral("Snapshot capture already in progress"));
        return;
    }

    SnapshotCaptureRequest request;
    request.mediaId = mediaId;
    request.filePath = filePath;
    request.ffmpegProgram = ffmpegProgram();
    request.outputRootPath = SnapshotService::defaultSnapshotRoot();
    request.thumbnailRootPath = ThumbnailService::defaultThumbnailRoot();
    request.thumbnailMaxWidth = 320;
    request.timestampMs = std::max<qint64>(0, timestampMs);

    m_snapshotMediaId = mediaId;
    m_snapshotResult = std::make_shared<SnapshotCaptureResult>();
    m_snapshotCancellation = std::make_shared<CancellationToken>();
    const std::shared_ptr<SnapshotCaptureResult> snapshotResult = m_snapshotResult;
    const std::shared_ptr<CancellationToken> snapshotCancellation = m_snapshotCancellation;
    setSnapshotState(true, QStringLiteral("Capturing snapshot: %1").arg(fileName.isEmpty() ? filePath : fileName));
    m_snapshotWatcher->setFuture(QtConcurrent::run([request, snapshotResult, snapshotCancellation]() {
        SnapshotService snapshotService;
        CaptureSnapshotUseCase useCase(&snapshotService);
        *snapshotResult = useCase.execute(request, snapshotCancellation);
    }));
}

void AppController::cancelSnapshotCapture()
{
    if (!m_snapshotInProgress || !m_snapshotCancellation) {
        return;
    }

    m_snapshotCancellation->cancel();
    setSnapshotState(true, QStringLiteral("Canceling snapshot..."));
}

void AppController::cancelActiveWork()
{
    cancelScan();
    cancelMetadataRefresh();
    cancelSnapshotCapture();
    cancelThumbnailMaintenance();
}

void AppController::rebuildThumbnailCache()
{
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) {
        setThumbnailMaintenanceState(false, QStringLiteral("Database is not ready for thumbnail rebuild."));
        return;
    }
    if (hasActiveBackgroundWork()) {
        setThumbnailMaintenanceState(m_thumbnailMaintenanceInProgress, QStringLiteral("Wait for active work to finish before rebuilding thumbnails."));
        return;
    }

    const QVector<ThumbnailBackfillItem> items = m_mediaRepository->fetchThumbnailBackfillItems();
    if (!m_mediaRepository->lastError().isEmpty()) {
        setThumbnailMaintenanceState(false, QStringLiteral("Thumbnail rebuild failed: %1").arg(m_mediaRepository->lastError()));
        return;
    }
    if (items.isEmpty()) {
        setThumbnailMaintenanceState(false, QStringLiteral("No thumbnails to rebuild."));
        return;
    }

    m_thumbnailMaintenanceCancellation = std::make_shared<CancellationToken>();
    m_thumbnailMaintenanceResult = std::make_shared<QVector<ThumbnailGenerationResult>>();
    const std::shared_ptr<CancellationToken> cancellation = m_thumbnailMaintenanceCancellation;
    const std::shared_ptr<QVector<ThumbnailGenerationResult>> results = m_thumbnailMaintenanceResult;
    const QPointer<AppController> self(this);
    const QString thumbnailRoot = ThumbnailService::defaultThumbnailRoot();
    setThumbnailMaintenanceState(true, QStringLiteral("Rebuilding thumbnail cache: 0/%1").arg(items.size()));

    m_thumbnailMaintenanceWatcher->setFuture(QtConcurrent::run([items, cancellation, results, self, thumbnailRoot]() {
        ThumbnailService thumbnailService;
        int processedCount = 0;
        for (const ThumbnailBackfillItem &item : items) {
            if (cancellation && cancellation->isCancellationRequested()) {
                break;
            }

            ThumbnailGenerationResult result;
            result.mediaId = item.mediaId;
            if (ThumbnailService::isUsableThumbnail(item.existingThumbnailPath, 320)) {
                result.thumbnailPath = item.existingThumbnailPath;
                result.skipped = true;
                result.succeeded = true;
            } else {
                ThumbnailGenerationRequest request;
                request.mediaId = item.mediaId;
                request.sourceImagePath = item.sourceImagePath;
                request.outputRootPath = thumbnailRoot;
                request.maxWidth = 320;
                request.quality = 82;
                result = thumbnailService.generate(request, cancellation);
            }

            if (result.succeeded && !result.skipped) {
                results->append(result);
            }

            ++processedCount;
            if (self) {
                QMetaObject::invokeMethod(self.data(), [self, processedCount, total = items.size()]() {
                    if (self) {
                        self->setThumbnailMaintenanceState(
                            true,
                            QStringLiteral("Rebuilding thumbnail cache: %1/%2").arg(processedCount).arg(total));
                    }
                }, Qt::QueuedConnection);
            }
        }
    }));
}

void AppController::cancelThumbnailMaintenance()
{
    if (!m_thumbnailMaintenanceInProgress || !m_thumbnailMaintenanceCancellation) {
        return;
    }

    m_thumbnailMaintenanceCancellation->cancel();
    setThumbnailMaintenanceState(true, QStringLiteral("Canceling thumbnail rebuild..."));
}

void AppController::resetLibrary()
{
    if (!m_databaseReady || !m_mediaRepository || !m_mediaLibraryModel) {
        setLibraryStatus(QStringLiteral("Database is not ready for library reset."));
        return;
    }
    if (m_scanController && m_scanController->isRunning()) {
        setLibraryStatus(QStringLiteral("Wait for scan to finish before resetting the library."));
        return;
    }
    if (m_libraryController && m_libraryController->isRunning()) {
        setLibraryStatus(QStringLiteral("Wait for library refresh to finish before resetting the library."));
        return;
    }
    if (m_metadataInProgress || (m_metadataWatcher && m_metadataWatcher->isRunning())) {
        setLibraryStatus(QStringLiteral("Wait for metadata refresh to finish before resetting the library."));
        return;
    }
    if (m_snapshotInProgress || (m_snapshotWatcher && m_snapshotWatcher->isRunning())) {
        setLibraryStatus(QStringLiteral("Wait for snapshot capture to finish before resetting the library."));
        return;
    }
    if (m_thumbnailMaintenanceInProgress || (m_thumbnailMaintenanceWatcher && m_thumbnailMaintenanceWatcher->isRunning())) {
        setLibraryStatus(QStringLiteral("Wait for thumbnail rebuild to finish before resetting the library."));
        return;
    }

    if (!m_mediaRepository->resetLibraryData()) {
        setLibraryStatus(QStringLiteral("Library reset failed: %1").arg(m_mediaRepository->lastError()));
        return;
    }

    QString snapshotClearError;
    const bool snapshotsCleared = SnapshotService::clearSnapshotRoot(SnapshotService::defaultSnapshotRoot(), &snapshotClearError);
    QString thumbnailClearError;
    const bool thumbnailsCleared = ThumbnailService::clearThumbnailRoot(ThumbnailService::defaultThumbnailRoot(), &thumbnailClearError);

    m_selectedIndex = -1;
    m_selectedMediaId = -1;
    m_metadataMediaId = -1;
    m_metadataManual = false;
    m_autoMetadataQueued = false;
    if (m_libraryController) {
        m_libraryController->cancelPending();
    }
    m_metadataResult.reset();
    m_snapshotMediaId = -1;
    m_snapshotResult.reset();
    m_thumbnailMaintenanceResult.reset();
    m_metadataCancellation.reset();
    m_snapshotCancellation.reset();
    m_thumbnailMaintenanceCancellation.reset();
    m_currentScanRoot.clear();
    if (m_scanController) {
        m_scanController->clearCurrentRoot();
    }
    m_autoMetadataFailures.clear();
    m_selectedSnapshots.clear();
    m_scanVisitedCount = 0;
    m_scanFoundCount = 0;
    m_scanProgressText.clear();
    m_mediaLibraryModel->setItems({});

    emit selectedIndexChanged();
    emit selectedMediaChanged();
    setScanState(false, QStringLiteral("No scan root"), {});
    setMetadataState(false, {});
    const QString snapshotResetStatus = snapshotsCleared
        ? QString()
        : QStringLiteral("Snapshot cleanup failed: %1").arg(snapshotClearError);
    setSnapshotState(false, snapshotResetStatus);
    const QString thumbnailResetStatus = thumbnailsCleared
        ? QString()
        : QStringLiteral("Thumbnail cleanup failed: %1").arg(thumbnailClearError);
    setThumbnailMaintenanceState(false, thumbnailResetStatus);
    setDetailStatus({});
    setFileActionStatus(QStringLiteral("Library reset complete"));
    setLibraryStatus(QStringLiteral("Library reset complete: 0 item(s)"));
}

QVariantMap AppController::aboutInfo() const
{
    return {
        {QStringLiteral("applicationName"), QCoreApplication::applicationName()},
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion().isEmpty()
                ? QStringLiteral("0.1")
                : QCoreApplication::applicationVersion()},
        {QStringLiteral("qtVersion"), QLibraryInfo::version().toString()},
        {QStringLiteral("databasePath"), m_databasePath},
    };
}

QVariantMap AppController::systemInfo() const
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
        {QStringLiteral("databaseReady"), m_databaseReady},
        {QStringLiteral("databaseStatus"), m_databaseStatus},
        {QStringLiteral("databasePath"), m_databasePath},
        {QStringLiteral("currentScanRoot"), m_currentScanRoot},
        {QStringLiteral("libraryItems"), m_mediaLibraryModel ? m_mediaLibraryModel->rowCount() : 0},
        {QStringLiteral("scanInProgress"), m_scanInProgress},
        {QStringLiteral("metadataInProgress"), m_metadataInProgress},
        {QStringLiteral("snapshotInProgress"), m_snapshotInProgress},
        {QStringLiteral("thumbnailMaintenanceInProgress"), m_thumbnailMaintenanceInProgress},
        {QStringLiteral("snapshotRoot"), SnapshotService::defaultSnapshotRoot()},
        {QStringLiteral("thumbnailRoot"), ThumbnailService::defaultThumbnailRoot()},
        {QStringLiteral("logPath"), AppLogger::logPath()},
        {QStringLiteral("rotatedLogPath"), AppLogger::rotatedLogPath()},
        {QStringLiteral("appDataPath"), PathSecurity::appDataPath()},
        {QStringLiteral("ffprobePath"), m_appSettings.ffprobePath},
        {QStringLiteral("ffmpegPath"), m_appSettings.ffmpegPath},
        {QStringLiteral("toolsStatus"), m_toolsStatus},
        {QStringLiteral("settingsStatus"), m_settingsStatus},
    };
}

void AppController::saveSettings(const QVariantMap &settings)
{
    AppSettings nextSettings = m_appSettings;
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

    nextSettings = AppSettingsRepository::normalized(nextSettings);
    const bool libraryQueryChanged = m_librarySortKey != nextSettings.sortKey
        || m_librarySortAscending != nextSettings.sortAscending;
    const bool thumbnailVisibilityChanged = m_showThumbnails != nextSettings.showThumbnails;

    m_appSettings = nextSettings;
    m_librarySortKey = m_appSettings.sortKey;
    m_librarySortAscending = m_appSettings.sortAscending;
    m_showThumbnails = m_appSettings.showThumbnails;

    saveCurrentSettings(QStringLiteral("Settings saved"));
    emit settingsStateChanged();
    if (libraryQueryChanged || thumbnailVisibilityChanged) {
        emit libraryStateChanged();
    }
    if (libraryQueryChanged) {
        requestLibraryReload(0);
    }
}

void AppController::resetToolPathsToPath()
{
    m_appSettings.ffprobePath.clear();
    m_appSettings.ffmpegPath.clear();
    saveCurrentSettings(QStringLiteral("Tool paths reset to PATH"));
    emit settingsStateChanged();
}

void AppController::validateExternalTools()
{
    ExternalToolService service;
    m_ffprobeToolStatus = service.validateFfprobe(m_appSettings.ffprobePath);
    m_ffmpegToolStatus = service.validateFfmpeg(m_appSettings.ffmpegPath);

    const QString ffprobeText = m_ffprobeToolStatus.succeeded
        ? QStringLiteral("ffprobe OK")
        : QStringLiteral("ffprobe unavailable");
    const QString ffmpegText = m_ffmpegToolStatus.succeeded
        ? QStringLiteral("ffmpeg OK")
        : QStringLiteral("ffmpeg unavailable");
    const bool allOk = m_ffprobeToolStatus.succeeded && m_ffmpegToolStatus.succeeded;
    setToolsStatus(QStringLiteral("%1, %2").arg(ffprobeText, ffmpegText));
    recordWorkEvent(QStringLiteral("tools"), m_toolsStatus, !allOk);
}

void AppController::openLogFile()
{
    const QString path = AppLogger::logPath();
    if (!QFileInfo::exists(path)) {
        setSettingsStatus(QStringLiteral("Log file does not exist yet."));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        setSettingsStatus(QStringLiteral("Could not open log file."));
        return;
    }

    setSettingsStatus(QStringLiteral("Log file opened"));
}

void AppController::openLogFolder()
{
    const QString path = QFileInfo(AppLogger::logPath()).absolutePath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        setSettingsStatus(QStringLiteral("Could not open log folder."));
        return;
    }

    setSettingsStatus(QStringLiteral("Log folder opened"));
}

void AppController::openAppDataFolder()
{
    const QString path = PathSecurity::appDataPath();
    QDir().mkpath(path);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        setSettingsStatus(QStringLiteral("Could not open app data folder."));
        return;
    }

    setSettingsStatus(QStringLiteral("App data folder opened"));
}

QString AppController::diagnosticReport() const
{
    QString report;
    QTextStream stream(&report);
    const QVariantMap info = systemInfo();
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
    stream << "ffprobe path: " << (m_appSettings.ffprobePath.isEmpty() ? QStringLiteral("PATH") : PathSecurity::redactedLocalPath(m_appSettings.ffprobePath)) << '\n';
    stream << "ffmpeg path: " << (m_appSettings.ffmpegPath.isEmpty() ? QStringLiteral("PATH") : PathSecurity::redactedLocalPath(m_appSettings.ffmpegPath)) << '\n';
    stream << "Tools: " << m_toolsStatus << '\n';
    stream << "Settings: " << m_settingsStatus << '\n';
    stream << "Active work: scan=" << m_scanInProgress
           << ", metadata=" << m_metadataInProgress
           << ", snapshot=" << m_snapshotInProgress
           << ", thumbnail=" << m_thumbnailMaintenanceInProgress << '\n';
    stream << "\nRecent work events:\n";
    for (const QVariant &eventValue : m_workEvents) {
        const QVariantMap event = eventValue.toMap();
        stream << "- " << event.value(QStringLiteral("timestamp")).toString()
               << " [" << event.value(QStringLiteral("kind")).toString() << "] "
               << event.value(QStringLiteral("message")).toString() << '\n';
    }

    return report;
}

void AppController::clearWorkEvents()
{
    if (m_workEvents.isEmpty()) {
        return;
    }

    m_workEvents.clear();
    emit workEventsChanged();
}

QString AppController::localPathFromUrl(const QUrl &url) const
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) {
        path = url.toString(QUrl::PreferLocalFile);
    }

    return QDir::toNativeSeparators(path);
}

void AppController::startDirectoryScanPath(const QString &rootPath)
{
    if (m_scanInProgress) {
        setScanState(true, QStringLiteral("Scan already in progress"), m_currentScanRoot);
        return;
    }
    if (hasActiveBackgroundWork()) {
        setScanState(false, QStringLiteral("Wait for active work to finish before scanning"), m_currentScanRoot);
        return;
    }

    if (!m_databaseReady || !m_mediaRepository) {
        setScanState(false, QStringLiteral("Database is not ready for scanning"), m_currentScanRoot);
        return;
    }
    if (m_databasePath.trimmed().isEmpty()) {
        setScanState(false, QStringLiteral("Database path is not available for scanning"), m_currentScanRoot);
        return;
    }

    if (m_scanController) {
        m_scanController->startScan(rootPath, m_databasePath);
    }
}

void AppController::handleScanFinished(const ScanCommitResult &commitResult)
{
    const DirectoryScanResult scanResult = commitResult.scan;
    const QString scanRoot = scanResult.rootPath.isEmpty() ? m_currentScanRoot : scanResult.rootPath;
    setScanProgress(scanResult.visitedFileCount, scanResult.matchedFileCount, m_scanProgressText);

    if (scanResult.canceled) {
        setScanState(false, QStringLiteral("Scan canceled"), scanRoot);
        setScanProgress(scanResult.visitedFileCount, scanResult.matchedFileCount, QStringLiteral("Scan canceled"));
        return;
    }

    if (!scanResult.succeeded) {
        setScanState(false, QStringLiteral("Scan failed: %1").arg(scanResult.errorMessage), scanRoot);
        return;
    }

    if (!commitResult.succeeded) {
        setScanState(false, QStringLiteral("DB update failed: %1").arg(commitResult.errorMessage), scanRoot);
        return;
    }

    requestLibraryReload(0);

    setScanState(
        false,
        QStringLiteral("Scan complete: %1 video file(s)").arg(commitResult.upsert.upsertedFileCount),
        scanRoot);
    setScanProgress(scanResult.visitedFileCount, scanResult.matchedFileCount, QStringLiteral("Scan complete"));
    if (!scanRoot.isEmpty() && m_appSettings.lastOpenFolder != scanRoot) {
        m_appSettings.lastOpenFolder = scanRoot;
        emit settingsStateChanged();
        saveCurrentSettings();
    }
}

void AppController::handleMetadataFinished()
{
    const MetadataExtractionResult result = m_metadataResult ? *m_metadataResult : MetadataExtractionResult{};
    const int mediaId = m_metadataMediaId;
    const bool wasManual = m_metadataManual;

    m_metadataMediaId = -1;
    m_metadataManual = false;
    m_metadataResult.reset();
    m_metadataCancellation.reset();

    if (result.canceled) {
        setMetadataState(false, QStringLiteral("Metadata canceled"));
        maybeStartAutoMetadataExtraction();
        return;
    }

    if (!result.succeeded) {
        if (!wasManual && mediaId > 0) {
            if (!m_autoMetadataFailures.contains(mediaId)) {
                m_autoMetadataFailures.append(mediaId);
            }
        }

        setMetadataState(false, QStringLiteral("Metadata failed: %1").arg(result.errorMessage));
        maybeStartAutoMetadataExtraction();
        return;
    }

    if (!m_mediaRepository || !m_mediaRepository->updateMediaMetadata(mediaId, result.metadata)) {
        setMetadataState(false, QStringLiteral("Metadata DB update failed: %1").arg(m_mediaRepository ? m_mediaRepository->lastError() : QStringLiteral("repository unavailable")));
        maybeStartAutoMetadataExtraction();
        return;
    }

    requestLibraryReload(0);
    setMetadataState(false, QStringLiteral("Metadata updated"));
    maybeStartAutoMetadataExtraction();
}

void AppController::handleSnapshotFinished()
{
    const SnapshotCaptureResult result = m_snapshotResult ? *m_snapshotResult : SnapshotCaptureResult{};
    const int mediaId = m_snapshotMediaId;
    m_snapshotMediaId = -1;
    m_snapshotResult.reset();
    m_snapshotCancellation.reset();

    if (result.canceled) {
        setSnapshotState(false, QStringLiteral("Snapshot canceled"));
        return;
    }

    if (!result.succeeded) {
        const QString errorMessage = result.errorMessage.isEmpty()
            ? QStringLiteral("Snapshot capture did not return a result.")
            : result.errorMessage;
        setSnapshotState(false, QStringLiteral("Snapshot failed: %1").arg(errorMessage));
        return;
    }

    if (!m_mediaRepository || !m_mediaRepository->addSnapshot(result.mediaId, result.imagePath, result.timestampMs)) {
        setSnapshotState(false, QStringLiteral("Snapshot DB insert failed: %1").arg(m_mediaRepository ? m_mediaRepository->lastError() : QStringLiteral("repository unavailable")));
        return;
    }

    const QString thumbnailPath = result.thumbnailPath.isEmpty() ? result.imagePath : result.thumbnailPath;
    if (!m_mediaRepository->setMediaThumbnailPath(result.mediaId, thumbnailPath)) {
        setSnapshotState(false, QStringLiteral("Thumbnail update failed: %1").arg(m_mediaRepository->lastError()));
        return;
    }

    if (m_mediaLibraryModel) {
        m_mediaLibraryModel->setThumbnailPath(result.mediaId, thumbnailPath);
    }

    if (m_selectedMediaId == result.mediaId || mediaId == result.mediaId) {
        refreshSelectedSnapshots();
        emit selectedMediaChanged();
    }

    setSnapshotState(false, QStringLiteral("Snapshot captured"));
}

void AppController::handleThumbnailMaintenanceFinished()
{
    const QVector<ThumbnailGenerationResult> results = m_thumbnailMaintenanceResult
        ? *m_thumbnailMaintenanceResult
        : QVector<ThumbnailGenerationResult> {};
    const bool wasCanceled = m_thumbnailMaintenanceCancellation
        && m_thumbnailMaintenanceCancellation->isCancellationRequested();
    m_thumbnailMaintenanceResult.reset();
    m_thumbnailMaintenanceCancellation.reset();

    int updatedCount = 0;
    for (const ThumbnailGenerationResult &result : results) {
        if (!result.succeeded || result.thumbnailPath.isEmpty()) {
            continue;
        }

        if (m_mediaRepository && m_mediaRepository->setMediaThumbnailPath(result.mediaId, result.thumbnailPath)) {
            ++updatedCount;
            if (m_mediaLibraryModel) {
                m_mediaLibraryModel->setThumbnailPath(result.mediaId, result.thumbnailPath);
            }
        }
    }

    if (updatedCount > 0) {
        emit selectedMediaChanged();
    }

    setThumbnailMaintenanceState(
        false,
        wasCanceled
            ? QStringLiteral("Thumbnail rebuild canceled: %1 updated").arg(updatedCount)
            : QStringLiteral("Thumbnail rebuild complete: %1 updated").arg(updatedCount));
}

void AppController::maybeStartAutoMetadataExtraction()
{
    if (m_autoMetadataQueued || !m_databaseReady || !m_mediaRepository) {
        return;
    }

    m_autoMetadataQueued = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_autoMetadataQueued = false;
        runAutoMetadataExtraction();
    }, Qt::QueuedConnection);
}

void AppController::runAutoMetadataExtraction()
{
    if (!m_databaseReady || !m_mediaRepository || m_metadataInProgress || (m_metadataWatcher && m_metadataWatcher->isRunning())) {
        return;
    }

    const QVariantMap media = selectedMedia();
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    if (mediaId <= 0 || m_autoMetadataFailures.contains(mediaId) || !mediaNeedsMetadata(media)) {
        return;
    }

    startMetadataExtraction(media, false);
}

void AppController::startMetadataExtraction(const QVariantMap &media, bool manual)
{
    const int mediaId = media.value(QStringLiteral("id")).toInt();
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    const QString fileName = media.value(QStringLiteral("fileName")).toString();

    if (mediaId <= 0 || filePath.isEmpty()) {
        setMetadataState(false, QStringLiteral("Select a media item before refreshing metadata."));
        return;
    }

    if (!m_databaseReady || !m_mediaRepository) {
        setMetadataState(false, QStringLiteral("Database is not ready for metadata updates."));
        return;
    }

    if (m_scanInProgress || m_snapshotInProgress || m_thumbnailMaintenanceInProgress) {
        setMetadataState(false, QStringLiteral("Wait for active work to finish before reading metadata."));
        return;
    }

    if (m_metadataInProgress || (m_metadataWatcher && m_metadataWatcher->isRunning())) {
        setMetadataState(true, QStringLiteral("Metadata refresh already in progress"));
        return;
    }

    if (manual) {
        m_autoMetadataFailures.removeAll(mediaId);
    }

    m_metadataMediaId = mediaId;
    m_metadataManual = manual;
    m_metadataResult = std::make_shared<MetadataExtractionResult>();
    m_metadataCancellation = std::make_shared<CancellationToken>();
    const std::shared_ptr<MetadataExtractionResult> metadataResult = m_metadataResult;
    const std::shared_ptr<CancellationToken> metadataCancellation = m_metadataCancellation;
    setMetadataState(true, QStringLiteral("Reading metadata: %1").arg(fileName.isEmpty() ? filePath : fileName));

    const QString configuredFfprobeProgram = ffprobeProgram();
    m_metadataWatcher->setFuture(QtConcurrent::run([filePath, configuredFfprobeProgram, metadataResult, metadataCancellation]() {
        MetadataService metadataService;
        ExtractMetadataUseCase useCase(&metadataService);
        *metadataResult = useCase.execute(filePath, configuredFfprobeProgram, metadataCancellation);
    }));
}

void AppController::refreshSelectedSnapshots()
{
    QVariantList nextSnapshots;
    if (m_databaseReady && m_mediaRepository && m_selectedMediaId > 0) {
        const QVector<SnapshotItem> snapshots = m_mediaRepository->fetchSnapshotsForMedia(m_selectedMediaId);
        if (m_mediaRepository->lastError().isEmpty()) {
            for (const SnapshotItem &snapshot : snapshots) {
                nextSnapshots.append(snapshotToMap(snapshot));
            }
        }
    }

    if (m_selectedSnapshots == nextSnapshots) {
        return;
    }

    m_selectedSnapshots = nextSnapshots;
    emit snapshotStateChanged();
}

bool AppController::hasActiveBackgroundWork() const
{
    return m_scanInProgress
        || m_metadataInProgress
        || m_snapshotInProgress
        || m_thumbnailMaintenanceInProgress
        || (m_scanController && m_scanController->isRunning())
        || (m_libraryController && m_libraryController->isRunning())
        || (m_metadataWatcher && m_metadataWatcher->isRunning())
        || (m_snapshotWatcher && m_snapshotWatcher->isRunning())
        || (m_thumbnailMaintenanceWatcher && m_thumbnailMaintenanceWatcher->isRunning());
}

void AppController::requestLibraryReload(int delayMs)
{
    if (!m_databaseReady || m_databasePath.trimmed().isEmpty()) {
        return;
    }

    if (m_libraryController) {
        m_libraryController->requestReload(m_databasePath, libraryQuery(), delayMs);
    }
}

void AppController::handleLibraryReloadFinished(const LibraryLoadResult &result)
{
    if (!result.succeeded) {
        setLibraryStatus(QStringLiteral("Library refresh failed: %1").arg(result.errorMessage));
        return;
    }

    const int preservedMediaId = m_selectedMediaId;
    applyLibraryItems(QVector<MediaLibraryItem>(result.items));
    m_selectedMediaId = preservedMediaId;
    syncSelectionAfterLibraryChange();
}

bool AppController::applyLibraryItems(QVector<MediaLibraryItem> items)
{
    if (!m_mediaLibraryModel) {
        return false;
    }

    m_mediaLibraryModel->setItems(std::move(items));
    setLibraryStatus(QStringLiteral("Showing %1 item(s)").arg(m_mediaLibraryModel->rowCount()));
    return true;
}

void AppController::syncSelectionAfterLibraryChange()
{
    int nextSelectedIndex = m_mediaLibraryModel && m_selectedMediaId > 0
        ? m_mediaLibraryModel->indexOfId(m_selectedMediaId)
        : m_selectedIndex;

    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) {
        nextSelectedIndex = -1;
    } else if (nextSelectedIndex < 0 || nextSelectedIndex >= m_mediaLibraryModel->rowCount()) {
        nextSelectedIndex = 0;
    }

    const int nextSelectedMediaId = nextSelectedIndex >= 0
        ? m_mediaLibraryModel->mediaIdAt(nextSelectedIndex)
        : -1;

    if (m_selectedIndex != nextSelectedIndex) {
        m_selectedIndex = nextSelectedIndex;
        emit selectedIndexChanged();
    }

    m_selectedMediaId = nextSelectedMediaId;
    emit selectedMediaChanged();
    refreshSelectedSnapshots();
    maybeStartAutoMetadataExtraction();
}

void AppController::setScanState(bool inProgress, const QString &status, const QString &currentScanRoot)
{
    if (m_scanInProgress == inProgress && m_scanStatus == status && m_currentScanRoot == currentScanRoot) {
        return;
    }

    m_scanInProgress = inProgress;
    m_scanStatus = status;
    m_currentScanRoot = currentScanRoot;
    emit scanStateChanged();
    if (!status.isEmpty()) {
        recordWorkEvent(QStringLiteral("scan"), status, statusLooksLikeWarning(status));
    }
}

void AppController::setScanProgress(int visitedCount, int foundCount, const QString &progressText)
{
    if (m_scanVisitedCount == visitedCount
        && m_scanFoundCount == foundCount
        && m_scanProgressText == progressText) {
        return;
    }

    m_scanVisitedCount = visitedCount;
    m_scanFoundCount = foundCount;
    m_scanProgressText = progressText;
    emit scanStateChanged();
}

void AppController::setLibraryStatus(const QString &status)
{
    if (m_libraryStatus == status) {
        return;
    }

    m_libraryStatus = status;
    emit libraryStateChanged();
}

void AppController::setMetadataState(bool inProgress, const QString &status)
{
    if (m_metadataInProgress == inProgress && m_metadataStatus == status) {
        return;
    }

    m_metadataInProgress = inProgress;
    m_metadataStatus = status;
    emit metadataStateChanged();
    if (!status.isEmpty()) {
        recordWorkEvent(QStringLiteral("metadata"), status, statusLooksLikeWarning(status));
    }
}

void AppController::setDetailStatus(const QString &status)
{
    if (m_detailStatus == status) {
        return;
    }

    m_detailStatus = status;
    emit detailStateChanged();
}

void AppController::setFileActionStatus(const QString &status)
{
    if (m_fileActionStatus == status) {
        return;
    }

    m_fileActionStatus = status;
    emit fileActionStateChanged();
}

void AppController::setSnapshotState(bool inProgress, const QString &status)
{
    if (m_snapshotInProgress == inProgress && m_snapshotStatus == status) {
        return;
    }

    m_snapshotInProgress = inProgress;
    m_snapshotStatus = status;
    emit snapshotStateChanged();
    if (!status.isEmpty()) {
        recordWorkEvent(QStringLiteral("snapshot"), status, statusLooksLikeWarning(status));
    }
}

void AppController::setThumbnailMaintenanceState(bool inProgress, const QString &status)
{
    if (m_thumbnailMaintenanceInProgress == inProgress && m_thumbnailMaintenanceStatus == status) {
        return;
    }

    m_thumbnailMaintenanceInProgress = inProgress;
    m_thumbnailMaintenanceStatus = status;
    emit thumbnailMaintenanceStateChanged();
    if (!status.isEmpty()) {
        recordWorkEvent(QStringLiteral("thumbnail"), status, statusLooksLikeWarning(status));
    }
}

void AppController::setSettingsStatus(const QString &status)
{
    if (m_settingsStatus == status) {
        return;
    }

    m_settingsStatus = status;
    emit settingsStateChanged();
    if (!status.isEmpty()) {
        recordWorkEvent(QStringLiteral("settings"), status, statusLooksLikeWarning(status));
    }
}

void AppController::setToolsStatus(const QString &status)
{
    if (m_toolsStatus == status) {
        return;
    }

    m_toolsStatus = status;
    emit settingsStateChanged();
}

bool AppController::saveCurrentSettings(const QString &successStatus)
{
    m_appSettings = AppSettingsRepository::normalized(m_appSettings);
    if (!m_settingsRepository) {
        if (!successStatus.isEmpty()) {
            setSettingsStatus(QStringLiteral("%1 (not persisted)").arg(successStatus));
        }
        return false;
    }

    if (!m_settingsRepository->save(m_appSettings)) {
        setSettingsStatus(QStringLiteral("Settings save failed: %1").arg(m_settingsRepository->lastError()));
        return false;
    }

    if (!successStatus.isEmpty()) {
        setSettingsStatus(successStatus);
    }
    return true;
}

QString AppController::ffprobeProgram() const
{
    return ExternalToolService::programForTool(QStringLiteral("ffprobe"), m_appSettings.ffprobePath);
}

QString AppController::ffmpegProgram() const
{
    return ExternalToolService::programForTool(QStringLiteral("ffmpeg"), m_appSettings.ffmpegPath);
}

void AppController::recordWorkEvent(const QString &kind, const QString &message, bool warning)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    QVariantMap event;
    event.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    event.insert(QStringLiteral("kind"), kind);
    event.insert(QStringLiteral("message"), message);
    event.insert(QStringLiteral("warning"), warning);
    m_workEvents.prepend(event);
    while (m_workEvents.size() > 200) {
        m_workEvents.removeLast();
    }

    if (warning) {
        qWarning().noquote() << QStringLiteral("[%1] %2").arg(kind, message);
    } else {
        qInfo().noquote() << QStringLiteral("[%1] %2").arg(kind, message);
    }
    emit workEventsChanged();
}

MediaLibraryQuery AppController::libraryQuery() const
{
    MediaLibraryQuery query;
    query.searchText = m_librarySearchText;
    query.sortKey = sortKeyFromString(m_librarySortKey);
    query.ascending = m_librarySortAscending;
    return query;
}

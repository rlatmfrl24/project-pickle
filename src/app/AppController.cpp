#include "AppController.h"

#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "media/ScanService.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

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

QString normalizedDirectoryPath(const QString &path)
{
    const QFileInfo fileInfo(path);
    QString normalizedPath = fileInfo.canonicalFilePath();
    if (normalizedPath.isEmpty()) {
        normalizedPath = fileInfo.absoluteFilePath();
    }

    return QDir::toNativeSeparators(normalizedPath);
}
}

AppController::AppController(
    MediaLibraryModel *mediaLibraryModel,
    MediaRepository *mediaRepository,
    QObject *parent)
    : QObject(parent)
    , m_mediaLibraryModel(mediaLibraryModel)
    , m_mediaRepository(mediaRepository)
{
    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) {
        m_selectedIndex = -1;
        m_selectedMediaId = -1;
    } else {
        m_selectedMediaId = m_mediaLibraryModel->mediaIdAt(m_selectedIndex);
    }
    m_libraryStatus = QStringLiteral("Showing %1 item(s)").arg(m_mediaLibraryModel ? m_mediaLibraryModel->rowCount() : 0);

    connect(&m_scanWatcher, &QFutureWatcher<DirectoryScanResult>::finished, this, &AppController::handleScanFinished);
}

AppController::~AppController()
{
    if (m_scanWatcher.isRunning()) {
        m_scanWatcher.waitForFinished();
    }
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

    if (!reloadLibraryFromRepository()) {
        setLibraryStatus(QStringLiteral("Library refresh failed: %1").arg(m_mediaRepository ? m_mediaRepository->lastError() : QStringLiteral("repository unavailable")));
    }
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
    emit libraryStateChanged();

    if (!reloadLibraryFromRepository()) {
        setLibraryStatus(QStringLiteral("Library refresh failed: %1").arg(m_mediaRepository ? m_mediaRepository->lastError() : QStringLiteral("repository unavailable")));
    }
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
    emit libraryStateChanged();

    if (!reloadLibraryFromRepository()) {
        setLibraryStatus(QStringLiteral("Library refresh failed: %1").arg(m_mediaRepository ? m_mediaRepository->lastError() : QStringLiteral("repository unavailable")));
    }
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
    emit libraryStateChanged();
}

QString AppController::libraryStatus() const
{
    return m_libraryStatus;
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

void AppController::startDirectoryScanPath(const QString &rootPath)
{
    if (rootPath.trimmed().isEmpty()) {
        setScanState(false, QStringLiteral("No folder selected"), m_currentScanRoot);
        return;
    }

    if (m_scanInProgress) {
        setScanState(true, QStringLiteral("Scan already in progress"), m_currentScanRoot);
        return;
    }

    if (!m_databaseReady || !m_mediaRepository) {
        setScanState(false, QStringLiteral("Database is not ready for scanning"), m_currentScanRoot);
        return;
    }

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        setScanState(false, QStringLiteral("Select a valid folder"), m_currentScanRoot);
        return;
    }

    const QString normalizedRootPath = normalizedDirectoryPath(rootPath);
    setScanState(true, QStringLiteral("Scanning %1").arg(normalizedRootPath), normalizedRootPath);

    m_scanWatcher.setFuture(QtConcurrent::run([normalizedRootPath]() {
        ScanService scanService;
        return scanService.scanDirectory(normalizedRootPath);
    }));
}

void AppController::handleScanFinished()
{
    const DirectoryScanResult scanResult = m_scanWatcher.result();
    const QString scanRoot = scanResult.rootPath.isEmpty() ? m_currentScanRoot : scanResult.rootPath;

    if (!scanResult.succeeded) {
        setScanState(false, QStringLiteral("Scan failed: %1").arg(scanResult.errorMessage), scanRoot);
        return;
    }

    MediaUpsertResult upsertResult;
    if (!m_mediaRepository->upsertScanResult(scanRoot, scanResult.files, &upsertResult)) {
        setScanState(false, QStringLiteral("DB update failed: %1").arg(m_mediaRepository->lastError()), scanRoot);
        return;
    }

    if (!reloadLibraryFromRepository()) {
        setScanState(false, QStringLiteral("Library refresh failed: %1").arg(m_mediaRepository->lastError()), scanRoot);
        return;
    }

    setScanState(
        false,
        QStringLiteral("Scan complete: %1 video file(s)").arg(upsertResult.upsertedFileCount),
        scanRoot);
}

bool AppController::reloadLibraryFromRepository()
{
    if (!m_mediaLibraryModel || !m_mediaRepository) {
        return false;
    }

    const int preservedMediaId = m_selectedMediaId;
    QVector<MediaLibraryItem> items = m_mediaRepository->fetchLibraryItems(libraryQuery());
    if (!m_mediaRepository->lastError().isEmpty()) {
        return false;
    }

    m_mediaLibraryModel->setItems(std::move(items));
    m_selectedMediaId = preservedMediaId;
    syncSelectionAfterLibraryChange();
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
}

void AppController::setLibraryStatus(const QString &status)
{
    if (m_libraryStatus == status) {
        return;
    }

    m_libraryStatus = status;
    emit libraryStateChanged();
}

MediaLibraryQuery AppController::libraryQuery() const
{
    MediaLibraryQuery query;
    query.searchText = m_librarySearchText;
    query.sortKey = sortKeyFromString(m_librarySortKey);
    query.ascending = m_librarySortAscending;
    return query;
}

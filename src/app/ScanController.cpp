#include "app/ScanController.h"

#include "application/ScanLibraryUseCase.h"
#include "core/CancellationToken.h"
#include "db/MediaRepository.h"
#include "db/ScopedDatabaseConnection.h"
#include "media/ScanService.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

namespace {
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

ScanController::ScanController(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<ScanCommitResult>::finished, this, &ScanController::handleFinished);
}

ScanController::~ScanController()
{
    disconnect(&m_watcher, nullptr, this, nullptr);
    if (m_cancellation) {
        m_cancellation->cancel();
    }
    waitForFinished();
}

bool ScanController::isRunning() const
{
    return m_watcher.isRunning();
}

bool ScanController::cancelAvailable() const
{
    return m_watcher.isRunning()
        && m_cancellation
        && !m_cancellation->isCancellationRequested();
}

void ScanController::startScan(const QString &rootPath, const QString &databasePath)
{
    if (rootPath.trimmed().isEmpty()) {
        emit scanRejected(QStringLiteral("No folder selected"), m_currentRoot);
        return;
    }

    if (m_watcher.isRunning()) {
        emit scanRejected(QStringLiteral("Scan already in progress"), m_currentRoot);
        return;
    }

    if (databasePath.trimmed().isEmpty()) {
        emit scanRejected(QStringLiteral("Database path is not available for scanning"), m_currentRoot);
        return;
    }

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit scanRejected(QStringLiteral("Select a valid folder"), m_currentRoot);
        return;
    }

    m_currentRoot = normalizedDirectoryPath(rootPath);
    m_cancellation = std::make_shared<CancellationToken>();
    const std::shared_ptr<CancellationToken> scanCancellation = m_cancellation;
    const QString normalizedRootPath = m_currentRoot;
    const QPointer<ScanController> self(this);

    emit scanStarted(QStringLiteral("Scanning %1").arg(normalizedRootPath), normalizedRootPath);
    emit progressChanged(0, 0, QStringLiteral("Scanning..."));

    m_watcher.setFuture(QtConcurrent::run([normalizedRootPath, databasePath, scanCancellation, self]() {
        ScanCommitResult commitResult;
        ScanService scanService;
        ScopedDatabaseConnection databaseConnection(databasePath, QStringLiteral("pickle_scan_worker"));
        if (!databaseConnection.isOpen()) {
            commitResult.errorMessage = databaseConnection.lastError();
            return commitResult;
        }

        MediaRepository repository(databaseConnection.database());
        ScanLibraryUseCase useCase(&scanService, &repository);
        commitResult = useCase.execute(
            normalizedRootPath,
            scanCancellation,
            [self](const ScanProgress &progress) {
                if (!self) {
                    return;
                }

                const QString path = QFileInfo(progress.currentPath).fileName();
                const QString text = progress.canceled
                    ? QStringLiteral("Canceling scan...")
                    : QStringLiteral("Scanned %1 file(s), found %2 video(s)%3")
                        .arg(progress.visitedFileCount)
                        .arg(progress.matchedFileCount)
                        .arg(path.isEmpty() ? QString() : QStringLiteral(" - %1").arg(path));
                QMetaObject::invokeMethod(self.data(), [self, progress, text]() {
                    if (self) {
                        emit self->progressChanged(progress.visitedFileCount, progress.matchedFileCount, text);
                    }
                }, Qt::QueuedConnection);
            });
        return commitResult;
    }));
}

bool ScanController::cancel()
{
    if (!cancelAvailable()) {
        return false;
    }

    m_cancellation->cancel();
    return true;
}

void ScanController::clearCurrentRoot()
{
    if (!m_watcher.isRunning()) {
        m_currentRoot.clear();
    }
}

void ScanController::waitForFinished()
{
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void ScanController::handleFinished()
{
    const ScanCommitResult result = m_watcher.result();
    m_cancellation.reset();
    emit scanFinished(result);
}

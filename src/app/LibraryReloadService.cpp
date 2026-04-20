#include "LibraryReloadService.h"

#include "application/LoadLibraryUseCase.h"
#include "db/MediaRepository.h"
#include "db/ScopedDatabaseConnection.h"

#include <QtConcurrent>

LibraryReloadService::LibraryReloadService(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &LibraryReloadService::startReload);
    connect(&m_watcher, &QFutureWatcher<LibraryLoadResult>::finished, this, &LibraryReloadService::handleFinished);
}

LibraryReloadService::~LibraryReloadService()
{
    disconnect(&m_watcher, nullptr, this, nullptr);
    m_timer.stop();
    waitForFinished();
}

bool LibraryReloadService::isRunning() const
{
    return m_watcher.isRunning();
}

void LibraryReloadService::requestReload(const QString &databasePath, const MediaLibraryQuery &query, int delayMs)
{
    if (databasePath.trimmed().isEmpty()) {
        return;
    }

    m_databasePath = databasePath;
    m_query = query;

    if (m_watcher.isRunning()) {
        m_queued = true;
        ++m_generation;
        emit statusChanged(QStringLiteral("Loading library..."));
        if (delayMs <= 0) {
            m_timer.stop();
        } else {
            m_timer.start(delayMs);
        }
        return;
    }

    if (delayMs <= 0) {
        m_timer.stop();
        startReload();
        return;
    }

    m_timer.start(delayMs);
}

void LibraryReloadService::cancelPending()
{
    m_timer.stop();
    m_queued = false;
}

void LibraryReloadService::waitForFinished()
{
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void LibraryReloadService::startReload()
{
    if (m_databasePath.trimmed().isEmpty()) {
        return;
    }

    if (m_watcher.isRunning()) {
        m_queued = true;
        ++m_generation;
        emit statusChanged(QStringLiteral("Loading library..."));
        return;
    }

    const int generation = ++m_generation;
    const QString databasePath = m_databasePath;
    const MediaLibraryQuery query = m_query;
    emit statusChanged(QStringLiteral("Loading library..."));

    m_watcher.setFuture(QtConcurrent::run([databasePath, query, generation]() {
        return LibraryReloadService::loadLibrary(databasePath, query, generation);
    }));
}

void LibraryReloadService::handleFinished()
{
    const LibraryLoadResult result = m_watcher.result();
    const bool shouldStartQueuedReload = m_queued;
    m_queued = false;

    if (result.generation == m_generation) {
        emit loadFinished(result);
    }

    if (shouldStartQueuedReload) {
        m_timer.stop();
        startReload();
    }
}

LibraryLoadResult LibraryReloadService::loadLibrary(
    const QString &databasePath,
    const MediaLibraryQuery &query,
    int generation)
{
    LibraryLoadResult result;
    result.query = query;
    result.generation = generation;

    ScopedDatabaseConnection databaseConnection(databasePath, QStringLiteral("pickle_library_worker"));
    if (!databaseConnection.isOpen()) {
        result.errorMessage = databaseConnection.lastError();
        return result;
    }

    MediaRepository repository(databaseConnection.database());
    LoadLibraryUseCase useCase(&repository);
    return useCase.execute(query, generation);
}

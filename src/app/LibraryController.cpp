#include "app/LibraryController.h"

#include "app/LibraryReloadService.h"

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_reloadService(std::make_unique<LibraryReloadService>())
{
    connect(m_reloadService.get(), &LibraryReloadService::statusChanged, this, &LibraryController::statusChanged);
    connect(m_reloadService.get(), &LibraryReloadService::loadFinished, this, &LibraryController::loadFinished);
}

LibraryController::~LibraryController()
{
    disconnect(m_reloadService.get(), nullptr, this, nullptr);
    waitForFinished();
}

bool LibraryController::isRunning() const
{
    return m_reloadService && m_reloadService->isRunning();
}

void LibraryController::requestReload(const QString &databasePath, const MediaLibraryQuery &query, int delayMs)
{
    if (m_reloadService) {
        m_reloadService->requestReload(databasePath, query, delayMs);
    }
}

void LibraryController::cancelPending()
{
    if (m_reloadService) {
        m_reloadService->cancelPending();
    }
}

void LibraryController::waitForFinished()
{
    if (m_reloadService) {
        m_reloadService->waitForFinished();
    }
}

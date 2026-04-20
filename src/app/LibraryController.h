#pragma once

#include "application/LibraryLoadResult.h"
#include "domain/LibraryQuery.h"

#include <QObject>
#include <QString>

#include <memory>

class LibraryReloadService;

class LibraryController : public QObject
{
    Q_OBJECT

public:
    explicit LibraryController(QObject *parent = nullptr);
    ~LibraryController() override;

    bool isRunning() const;
    void requestReload(const QString &databasePath, const MediaLibraryQuery &query, int delayMs);
    void cancelPending();
    void waitForFinished();

signals:
    void statusChanged(const QString &status);
    void loadFinished(const LibraryLoadResult &result);

private:
    std::unique_ptr<LibraryReloadService> m_reloadService;
};

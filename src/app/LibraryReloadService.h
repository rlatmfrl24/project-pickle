#pragma once

#include "media/MediaTypes.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

class LibraryReloadService : public QObject
{
    Q_OBJECT

public:
    explicit LibraryReloadService(QObject *parent = nullptr);
    ~LibraryReloadService() override;

    bool isRunning() const;
    void requestReload(const QString &databasePath, const MediaLibraryQuery &query, int delayMs);
    void cancelPending();
    void waitForFinished();

signals:
    void statusChanged(const QString &status);
    void loadFinished(const LibraryLoadResult &result);

private:
    void startReload();
    void handleFinished();
    static LibraryLoadResult loadLibrary(
        const QString &databasePath,
        const MediaLibraryQuery &query,
        int generation);

    QTimer m_timer;
    QFutureWatcher<LibraryLoadResult> m_watcher;
    QString m_databasePath;
    MediaLibraryQuery m_query;
    int m_generation = 0;
    bool m_queued = false;
};

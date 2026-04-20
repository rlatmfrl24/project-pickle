#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariant>

#include "app/AppController.h"
#include "app/MediaItemPresenter.h"
#include "core/AppLogger.h"
#include "db/AppSettingsRepository.h"
#include "db/DatabaseService.h"
#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/MediaLibraryModel.h"
#include "media/PlaybackController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Pickle"));
    QCoreApplication::setOrganizationName(QStringLiteral("Pickle"));
#ifdef PICKLE_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(PICKLE_VERSION));
#endif
    AppLogger::initialize();

    MediaLibraryModel mediaLibraryModel;

    DatabaseService databaseService;
    bool databaseReady = databaseService.open();
    QString databaseStatus;

    if (databaseReady) {
        MigrationService migrationService(databaseService.database());
        databaseReady = migrationService.migrate();
        databaseStatus = databaseReady
            ? QStringLiteral("DB ready")
            : QStringLiteral("DB migration failed: %1").arg(migrationService.lastError());
    } else {
        databaseStatus = QStringLiteral("DB open failed: %1").arg(databaseService.lastError());
    }

    MediaRepository mediaRepository(databaseService.database());
    AppSettingsRepository settingsRepository(databaseService.database());
    AppSettings appSettings;
    if (databaseReady) {
        appSettings = settingsRepository.load();
        if (!settingsRepository.lastError().isEmpty()) {
            qWarning() << "Settings load failed:" << settingsRepository.lastError();
        }
    }

    if (databaseReady) {
        MediaLibraryQuery query;
        if (appSettings.sortKey == QStringLiteral("size")) {
            query.sortKey = MediaLibrarySortKey::Size;
        } else if (appSettings.sortKey == QStringLiteral("modified")) {
            query.sortKey = MediaLibrarySortKey::Modified;
        }
        query.ascending = appSettings.sortAscending;
        mediaLibraryModel.setItems(MediaItemPresenter::present(mediaRepository.fetchMediaFiles(query)));
        if (!mediaRepository.lastError().isEmpty()) {
            databaseReady = false;
            databaseStatus = QStringLiteral("Library load failed: %1").arg(mediaRepository.lastError());
        }
    }

    AppController appController(
        &mediaLibraryModel,
        databaseReady ? &mediaRepository : nullptr,
        databaseReady ? &settingsRepository : nullptr);
    appController.setDatabaseState(databaseReady, databaseStatus, databaseService.databasePath());
    PlaybackController playbackController;
    playbackController.setMedia(appController.selectedMedia());
    QObject::connect(
        &appController,
        &AppController::selectedMediaChanged,
        &playbackController,
        [&appController, &playbackController]() {
            playbackController.setMedia(appController.selectedMedia());
        });

    if (databaseReady) {
        qInfo() << databaseStatus << databaseService.databasePath();
    } else {
        qWarning() << databaseStatus << databaseService.databasePath();
    }

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("mediaLibraryModel"), QVariant::fromValue(&mediaLibraryModel)},
        {QStringLiteral("appController"), QVariant::fromValue(&appController)},
        {QStringLiteral("playbackController"), QVariant::fromValue(&playbackController)},
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("pickle", "Main");

    return QCoreApplication::exec();
}

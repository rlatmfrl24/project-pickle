#include "app/AppController.h"
#include "core/ExternalToolService.h"
#include "db/AppSettingsRepository.h"
#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/MediaLibraryModel.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QtTest/QtTest>

namespace {
struct TestDatabase {
    QSqlDatabase database;
    QString connectionName;
};

TestDatabase createDatabase(const QString &databasePath)
{
    TestDatabase result;
    result.connectionName = QStringLiteral("m10_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    result.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), result.connectionName);
    result.database.setDatabaseName(databasePath);
    return result;
}
}

class M10SettingsDiagnosticsTest : public QObject
{
    Q_OBJECT

private slots:
    void settingsRepositoryStoresAndNormalizesValues();
    void externalToolServiceReportsStructuredValidationResults();
    void controllerPersistsSettingsAndBuildsDiagnostics();
    void controllerKeepsOnlyRecentWorkEvents();
};

void M10SettingsDiagnosticsTest::settingsRepositoryStoresAndNormalizesValues()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("settings.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        AppSettings settings;
        settings.ffprobePath = QStringLiteral("  ");
        settings.ffmpegPath = QStringLiteral("  C:/Tools/ffmpeg.exe  ");
        settings.showThumbnails = false;
        settings.sortKey = QStringLiteral("invalid");
        settings.sortAscending = false;
        settings.lastOpenFolder = QStringLiteral("  C:/Media  ");
        settings.playerVolume = 7.5;
        settings.playerMuted = true;

        AppSettingsRepository repository(testDatabase.database);
        QVERIFY2(repository.save(settings), qPrintable(repository.lastError()));

        AppSettings loaded = repository.load();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(loaded.ffprobePath, QString());
        QCOMPARE(loaded.ffmpegPath, QStringLiteral("C:/Tools/ffmpeg.exe"));
        QCOMPARE(loaded.showThumbnails, false);
        QCOMPARE(loaded.sortKey, QStringLiteral("name"));
        QCOMPARE(loaded.sortAscending, false);
        QCOMPARE(loaded.lastOpenFolder, QStringLiteral("C:/Media"));
        QCOMPARE(loaded.playerVolume, 1.0);
        QCOMPARE(loaded.playerMuted, true);

        settings.sortKey = QStringLiteral("modified");
        settings.playerVolume = -1.0;
        QVERIFY2(repository.save(settings), qPrintable(repository.lastError()));
        loaded = repository.load();
        QCOMPARE(loaded.sortKey, QStringLiteral("modified"));
        QCOMPARE(loaded.playerVolume, 0.0);

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M10SettingsDiagnosticsTest::externalToolServiceReportsStructuredValidationResults()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString missingFfmpeg = tempDir.filePath(QStringLiteral("missing-ffmpeg.exe"));
    ExternalToolService service;

    const ExternalToolStatus missingStatus = service.validateFfmpeg(missingFfmpeg);
    QCOMPARE(missingStatus.toolName, QStringLiteral("ffmpeg"));
    QCOMPARE(missingStatus.configuredPath, missingFfmpeg);
    QVERIFY(missingStatus.configuredPathUsed);
    QVERIFY(!missingStatus.succeeded);
    QVERIFY(!missingStatus.available);
    QVERIFY(missingStatus.errorMessage.contains(QStringLiteral("does not exist")));

    const ExternalToolStatus pathFallbackStatus = service.validateFfprobe(QString());
    QCOMPARE(pathFallbackStatus.toolName, QStringLiteral("ffprobe"));
    QCOMPARE(pathFallbackStatus.configuredPath, QString());
    QVERIFY(!pathFallbackStatus.configuredPathUsed);
    if (QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty()) {
        QCOMPARE(pathFallbackStatus.resolvedProgram, QStringLiteral("ffprobe"));
        QVERIFY(!pathFallbackStatus.succeeded);
        QVERIFY(pathFallbackStatus.errorMessage.contains(QStringLiteral("PATH")));
    } else {
        QVERIFY(QFileInfo(pathFallbackStatus.resolvedProgram).isAbsolute());
    }
    QVERIFY(pathFallbackStatus.succeeded || !pathFallbackStatus.errorMessage.isEmpty());
}

void M10SettingsDiagnosticsTest::controllerPersistsSettingsAndBuildsDiagnostics()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("controller.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        MediaRepository mediaRepository(testDatabase.database);
        MediaLibraryModel model;
        AppSettingsRepository settingsRepository(testDatabase.database);
        AppController controller(&model, &mediaRepository, &settingsRepository);
        controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());

        const QString missingFfprobe = databaseDir.filePath(QStringLiteral("missing-ffprobe.exe"));
        const QString missingFfmpeg = databaseDir.filePath(QStringLiteral("missing-ffmpeg.exe"));
        controller.saveSettings({
            {QStringLiteral("ffprobePath"), missingFfprobe},
            {QStringLiteral("ffmpegPath"), missingFfmpeg},
            {QStringLiteral("showThumbnails"), false},
            {QStringLiteral("sortKey"), QStringLiteral("size")},
            {QStringLiteral("sortAscending"), false},
            {QStringLiteral("lastOpenFolder"), databaseDir.path()},
            {QStringLiteral("playerVolume"), 0.25},
            {QStringLiteral("playerMuted"), true},
        });

        QCOMPARE(controller.ffprobePath(), missingFfprobe);
        QCOMPARE(controller.ffmpegPath(), missingFfmpeg);
        QCOMPARE(controller.showThumbnails(), false);
        QCOMPARE(controller.librarySortKey(), QStringLiteral("size"));
        QCOMPARE(controller.librarySortAscending(), false);
        QCOMPARE(controller.playerVolume(), 0.25);
        QCOMPARE(controller.playerMuted(), true);
        QCOMPARE(controller.lastOpenFolderUrl(), QUrl::fromLocalFile(databaseDir.path()));

        const AppSettings loadedSettings = settingsRepository.load();
        QCOMPARE(loadedSettings.sortKey, QStringLiteral("size"));
        QCOMPARE(loadedSettings.sortAscending, false);
        QCOMPARE(loadedSettings.showThumbnails, false);
        QCOMPARE(loadedSettings.playerVolume, 0.25);
        QCOMPARE(loadedSettings.playerMuted, true);

        controller.validateExternalTools();
        QVERIFY(controller.toolsStatus().contains(QStringLiteral("unavailable")));
        QVERIFY(!controller.workEvents().isEmpty());

        const QString report = controller.diagnosticReport();
        QVERIFY(report.contains(QStringLiteral("Pickle Diagnostic Report")));
        QVERIFY(report.contains(QStringLiteral("Tools:")));
        QVERIFY(report.contains(QStringLiteral("Recent work events")));
        QVERIFY(!report.contains(testDatabase.database.databaseName()));
        QVERIFY(report.contains(QStringLiteral("controller.sqlite3")));
        QVERIFY(report.contains(QStringLiteral("<home>/"))
            || report.contains(QStringLiteral("<app-data>/"))
            || report.contains(QStringLiteral("<path>/")));

        const QString mediaPath = QDir(databaseDir.path()).filePath(QStringLiteral("clip.mp4"));
        QCOMPARE(controller.localPathFromUrl(QUrl::fromLocalFile(mediaPath)), QDir::toNativeSeparators(mediaPath));

        controller.clearWorkEvents();
        QVERIFY(controller.workEvents().isEmpty());

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M10SettingsDiagnosticsTest::controllerKeepsOnlyRecentWorkEvents()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("events.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        MediaRepository mediaRepository(testDatabase.database);
        MediaLibraryModel model;
        AppSettingsRepository settingsRepository(testDatabase.database);
        AppController controller(&model, &mediaRepository, &settingsRepository);

        controller.saveSettings({
            {QStringLiteral("ffprobePath"), databaseDir.filePath(QStringLiteral("missing-ffprobe.exe"))},
            {QStringLiteral("ffmpegPath"), databaseDir.filePath(QStringLiteral("missing-ffmpeg.exe"))},
        });
        controller.clearWorkEvents();

        for (int i = 0; i < 205; ++i) {
            controller.validateExternalTools();
        }

        QCOMPARE(controller.workEvents().size(), 200);

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

QTEST_MAIN(M10SettingsDiagnosticsTest)

#include "M10SettingsDiagnosticsTest.moc"

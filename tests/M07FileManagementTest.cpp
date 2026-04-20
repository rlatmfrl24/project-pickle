#include "app/AppController.h"
#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/MediaLibraryModel.h"
#include "media/RenameService.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
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
    result.connectionName = QStringLiteral("m07_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    result.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), result.connectionName);
    result.database.setDatabaseName(databasePath);
    return result;
}

bool columnExists(QSqlDatabase database, const QString &tableName, const QString &columnName)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        return false;
    }

    while (query.next()) {
        if (query.value(QStringLiteral("name")).toString() == columnName) {
            return true;
        }
    }

    return false;
}

bool writeFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write("pickle");
    return true;
}

int rowCount(QSqlDatabase database, const QString &tableName)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName)) || !query.next()) {
        return -1;
    }

    return query.value(0).toInt();
}

int insertMedia(QSqlDatabase database, const QString &filePath, QString *errorMessage = nullptr)
{
    const QFileInfo fileInfo(filePath);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name,
            file_path,
            file_extension,
            file_size,
            modified_at,
            duration_ms,
            width,
            height,
            video_codec,
            audio_codec
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    query.addBindValue(fileInfo.fileName());
    query.addBindValue(QDir::toNativeSeparators(fileInfo.absoluteFilePath()));
    query.addBindValue(QStringLiteral(".%1").arg(fileInfo.suffix().toLower()));
    query.addBindValue(fileInfo.exists() ? fileInfo.size() : 1024);
    query.addBindValue(QStringLiteral("2026-04-20T00:00:00.000Z"));
    query.addBindValue(10000);
    query.addBindValue(1920);
    query.addBindValue(1080);
    query.addBindValue(QStringLiteral("h264"));
    query.addBindValue(QStringLiteral("aac"));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return query.lastInsertId().toInt();
}
}

class M07FileManagementTest : public QObject
{
    Q_OBJECT

private slots:
    void migrationAddsM07ColumnsToNewAndExistingDatabases();
    void repositoryStoresFlagsAndPlaybackPosition();
    void repositoryFetchesLargeTaggedLibraryQuickly();
    void renameServiceRenamesFileAndRejectsInvalidNames();
    void controllerPreservesSelectionAcrossFileActions();
    void modelExposesM07Roles();
};

void M07FileManagementTest::migrationAddsM07ColumnsToNewAndExistingDatabases()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase freshDatabase = createDatabase(databaseDir.filePath(QStringLiteral("fresh.sqlite3")));
    {
        QVERIFY(freshDatabase.database.open());
        MigrationService migrationService(freshDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));
        QVERIFY(columnExists(freshDatabase.database, QStringLiteral("media_files"), QStringLiteral("is_favorite")));
        QVERIFY(columnExists(freshDatabase.database, QStringLiteral("media_files"), QStringLiteral("is_delete_candidate")));
        freshDatabase.database.close();
    }
    freshDatabase.database = {};
    QSqlDatabase::removeDatabase(freshDatabase.connectionName);

    TestDatabase legacyDatabase = createDatabase(databaseDir.filePath(QStringLiteral("legacy.sqlite3")));
    {
        QVERIFY(legacyDatabase.database.open());
        QSqlQuery query(legacyDatabase.database);
        QVERIFY(query.exec(QStringLiteral(R"sql(
            CREATE TABLE schema_migrations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                version TEXT NOT NULL UNIQUE,
                applied_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO schema_migrations (version) VALUES ('001_initial_schema')")));
        QVERIFY(query.exec(QStringLiteral(R"sql(
            CREATE TABLE media_files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_name TEXT NOT NULL,
                file_path TEXT NOT NULL UNIQUE,
                file_extension TEXT,
                file_size INTEGER NOT NULL DEFAULT 0,
                modified_at TEXT,
                review_status TEXT NOT NULL DEFAULT 'unreviewed',
                last_played_at TEXT,
                last_position_ms INTEGER NOT NULL DEFAULT 0,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                updated_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql")));
        QVERIFY(query.exec(QStringLiteral(R"sql(
            CREATE TABLE tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                color TEXT,
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql")));

        MigrationService migrationService(legacyDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));
        QVERIFY(columnExists(legacyDatabase.database, QStringLiteral("media_files"), QStringLiteral("is_favorite")));
        QVERIFY(columnExists(legacyDatabase.database, QStringLiteral("media_files"), QStringLiteral("is_delete_candidate")));
        legacyDatabase.database.close();
    }
    legacyDatabase.database = {};
    QSqlDatabase::removeDatabase(legacyDatabase.connectionName);
}

void M07FileManagementTest::repositoryStoresFlagsAndPlaybackPosition()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        const QString mediaPath = databaseDir.filePath(QStringLiteral("flags.mp4"));
        QVERIFY(writeFile(mediaPath));
        QString insertError;
        const int mediaId = insertMedia(testDatabase.database, mediaPath, &insertError);
        QVERIFY2(mediaId > 0, qPrintable(insertError));

        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.updateMediaDetails(
                     mediaId,
                     QStringLiteral("note"),
                     QStringLiteral("reviewed"),
                     3,
                     {QStringLiteral("ResetTag")}),
                 qPrintable(repository.lastError()));
        QVERIFY2(repository.setMediaFavorite(mediaId, true), qPrintable(repository.lastError()));
        QVERIFY2(repository.setMediaDeleteCandidate(mediaId, true), qPrintable(repository.lastError()));
        QVERIFY2(repository.updatePlaybackPosition(mediaId, 12345, QStringLiteral("2026-04-20T10:00:00.000Z")), qPrintable(repository.lastError()));

        const QVector<MediaLibraryItem> items = repository.fetchLibraryItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), 1);
        QVERIFY(items.first().isFavorite);
        QVERIFY(items.first().isDeleteCandidate);
        QCOMPARE(items.first().lastPositionMs, 12345);
        QCOMPARE(items.first().lastPlayedAt, QStringLiteral("2026-04-20T10:00:00.000Z"));

        QVERIFY2(repository.resetLibraryData(), qPrintable(repository.lastError()));
        QCOMPARE(repository.fetchLibraryItems().size(), 0);
        QCOMPARE(rowCount(testDatabase.database, QStringLiteral("media_files")), 0);
        QCOMPARE(rowCount(testDatabase.database, QStringLiteral("media_tags")), 0);
        QCOMPARE(rowCount(testDatabase.database, QStringLiteral("tags")), 0);
        QCOMPARE(rowCount(testDatabase.database, QStringLiteral("scan_roots")), 0);

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M07FileManagementTest::repositoryFetchesLargeTaggedLibraryQuickly()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("large.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        QVERIFY(testDatabase.database.transaction());

        QVector<int> tagIds;
        for (int tagIndex = 0; tagIndex < 8; ++tagIndex) {
            QSqlQuery tagQuery(testDatabase.database);
            tagQuery.prepare(QStringLiteral("INSERT INTO tags (name) VALUES (?)"));
            tagQuery.addBindValue(QStringLiteral("tag-%1").arg(tagIndex));
            QVERIFY2(tagQuery.exec(), qPrintable(tagQuery.lastError().text()));
            tagIds.append(tagQuery.lastInsertId().toInt());
        }

        constexpr int MediaCount = 1500;
        for (int index = 0; index < MediaCount; ++index) {
            QSqlQuery mediaQuery(testDatabase.database);
            mediaQuery.prepare(QStringLiteral(R"sql(
                INSERT INTO media_files (
                    file_name,
                    file_path,
                    file_extension,
                    file_size,
                    modified_at,
                    description
                )
                VALUES (?, ?, ?, ?, ?, ?)
            )sql"));
            mediaQuery.addBindValue(QStringLiteral("video-%1.mp4").arg(index, 5, 10, QLatin1Char('0')));
            mediaQuery.addBindValue(QDir::toNativeSeparators(QStringLiteral("C:/Media/video-%1.mp4").arg(index, 5, 10, QLatin1Char('0'))));
            mediaQuery.addBindValue(QStringLiteral(".mp4"));
            mediaQuery.addBindValue(1024 + index);
            mediaQuery.addBindValue(QStringLiteral("2026-04-20T00:00:00.000Z"));
            mediaQuery.addBindValue(index % 10 == 0 ? QStringLiteral("searchable") : QStringLiteral(""));
            QVERIFY2(mediaQuery.exec(), qPrintable(mediaQuery.lastError().text()));
            const int mediaId = mediaQuery.lastInsertId().toInt();

            for (int offset = 0; offset < 3; ++offset) {
                QSqlQuery linkQuery(testDatabase.database);
                linkQuery.prepare(QStringLiteral("INSERT INTO media_tags (media_id, tag_id) VALUES (?, ?)"));
                linkQuery.addBindValue(mediaId);
                linkQuery.addBindValue(tagIds.at((index + offset) % tagIds.size()));
                QVERIFY2(linkQuery.exec(), qPrintable(linkQuery.lastError().text()));
            }
        }

        QVERIFY(testDatabase.database.commit());

        MediaRepository repository(testDatabase.database);
        QElapsedTimer timer;
        timer.start();
        const QVector<MediaLibraryItem> items = repository.fetchLibraryItems();
        const qint64 elapsedMs = timer.elapsed();

        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), MediaCount);
        QCOMPARE(items.first().tags.size(), 3);
        QVERIFY2(elapsedMs < 5000, qPrintable(QStringLiteral("Fetching %1 tagged items took %2 ms").arg(MediaCount).arg(elapsedMs)));

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M07FileManagementTest::renameServiceRenamesFileAndRejectsInvalidNames()
{
    QTemporaryDir mediaDir;
    QVERIFY(mediaDir.isValid());

    const QString originalPath = mediaDir.filePath(QStringLiteral("original.mp4"));
    const QString conflictPath = mediaDir.filePath(QStringLiteral("conflict.mp4"));
    QVERIFY(writeFile(originalPath));
    QVERIFY(writeFile(conflictPath));

    RenameService service;
    QVERIFY(!service.renameFile(originalPath, QString()).succeeded);
    QVERIFY(!service.renameFile(originalPath, QStringLiteral("bad/name")).succeeded);
    QVERIFY(!service.renameFile(originalPath, QStringLiteral("conflict")).succeeded);
    QVERIFY(QFileInfo::exists(originalPath));

    const FileRenameResult result = service.renameFile(originalPath, QStringLiteral("renamed"));
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(result.file.fileName, QStringLiteral("renamed.mp4"));
    QVERIFY(!QFileInfo::exists(originalPath));
    QVERIFY(QFileInfo::exists(result.file.filePath));
}

void M07FileManagementTest::controllerPreservesSelectionAcrossFileActions()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        const QString mediaPath = databaseDir.filePath(QStringLiteral("controller.mp4"));
        QVERIFY(writeFile(mediaPath));
        QString insertError;
        const int mediaId = insertMedia(testDatabase.database, mediaPath, &insertError);
        QVERIFY2(mediaId > 0, qPrintable(insertError));

        MediaRepository repository(testDatabase.database);
        MediaLibraryModel model;
        model.setItems(repository.fetchLibraryItems());
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));

        AppController controller(&model, &repository);
        controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());
        controller.selectIndex(0);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("id")).toInt(), mediaId);

        controller.setSelectedFavorite(true);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("id")).toInt(), mediaId);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("isFavorite")).toBool(), true);

        controller.setSelectedDeleteCandidate(true);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("id")).toInt(), mediaId);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("isDeleteCandidate")).toBool(), true);

        controller.saveSelectedPlaybackPosition(15000);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("id")).toInt(), mediaId);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("lastPositionMs")).toLongLong(), 15000);

        controller.renameSelectedMedia(QStringLiteral("controller-renamed"));
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("id")).toInt(), mediaId);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("fileName")).toString(), QStringLiteral("controller-renamed.mp4"));
        QVERIFY(QFileInfo::exists(databaseDir.filePath(QStringLiteral("controller-renamed.mp4"))));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.libraryStatus().contains(QStringLiteral("Loading")), 5000);

        controller.resetLibrary();
        QCOMPARE(controller.selectedIndex(), -1);
        QVERIFY(controller.selectedMedia().isEmpty());
        QCOMPARE(model.rowCount(), 0);

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M07FileManagementTest::modelExposesM07Roles()
{
    MediaLibraryModel model;
    MediaLibraryItem item;
    item.id = 7;
    item.fileName = QStringLiteral("role.mp4");
    item.isFavorite = true;
    item.isDeleteCandidate = true;
    item.lastPositionMs = 9876;
    item.lastPlayedAt = QStringLiteral("2026-04-20T10:00:00.000Z");
    model.setItems({item});

    const QVariantMap row = model.get(0);
    QCOMPARE(row.value(QStringLiteral("isFavorite")).toBool(), true);
    QCOMPARE(row.value(QStringLiteral("isDeleteCandidate")).toBool(), true);
    QCOMPARE(row.value(QStringLiteral("lastPositionMs")).toLongLong(), 9876);
    QCOMPARE(row.value(QStringLiteral("lastPlayedAt")).toString(), QStringLiteral("2026-04-20T10:00:00.000Z"));
}

QTEST_MAIN(M07FileManagementTest)

#include "M07FileManagementTest.moc"

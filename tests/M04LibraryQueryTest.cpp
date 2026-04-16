#include "app/AppController.h"
#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/MediaLibraryModel.h"
#include "media/PlaybackController.h"

#include <QDir>
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
    result.connectionName = QStringLiteral("m04_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    result.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), result.connectionName);
    result.database.setDatabaseName(databasePath);
    return result;
}

bool insertMedia(
    QSqlDatabase database,
    const QString &fileName,
    qint64 fileSize,
    const QString &modifiedAt,
    const QString &description = {},
    const QString &thumbnailPath = {},
    QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name,
            file_path,
            file_extension,
            file_size,
            modified_at,
            description,
            thumbnail_path
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )sql"));
    query.addBindValue(fileName);
    query.addBindValue(QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
    query.addBindValue(QStringLiteral(".mp4"));
    query.addBindValue(fileSize);
    query.addBindValue(modifiedAt);
    query.addBindValue(description.isNull() ? QStringLiteral("") : description);
    query.addBindValue(thumbnailPath);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QStringList itemNames(const QVector<MediaLibraryItem> &items)
{
    QStringList names;
    for (const MediaLibraryItem &item : items) {
        names.append(item.fileName);
    }

    return names;
}
}

class M04LibraryQueryTest : public QObject
{
    Q_OBJECT

private slots:
    void repositorySortsAndSearchesLibraryItems();
    void modelExposesSelectionHelpersAndM04Roles();
    void controllerPreservesSelectedMediaAcrossRefreshes();
    void playbackControllerExposesLocalFileSource();
};

void M04LibraryQueryTest::repositorySortsAndSearchesLibraryItems()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));

    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        QString insertError;
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("charlie.mp4"), 300, QStringLiteral("2026-04-03T00:00:00.000Z"), QStringLiteral("plain"), {}, &insertError), qPrintable(insertError));
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("alpha.mp4"), 100, QStringLiteral("2026-04-01T00:00:00.000Z"), QStringLiteral("target description"), {}, &insertError), qPrintable(insertError));
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("bravo.mp4"), 200, QStringLiteral("2026-04-02T00:00:00.000Z"), QStringLiteral("plain"), {}, &insertError), qPrintable(insertError));
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("wild%card.mp4"), 400, QStringLiteral("2026-04-04T00:00:00.000Z"), QStringLiteral("literal percent"), {}, &insertError), qPrintable(insertError));
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("wildXcard.mp4"), 500, QStringLiteral("2026-04-05T00:00:00.000Z"), QStringLiteral("wildcard neighbor"), {}, &insertError), qPrintable(insertError));

        MediaRepository repository(testDatabase.database);

        MediaLibraryQuery query;
        query.sortKey = MediaLibrarySortKey::Name;
        query.ascending = true;
        QCOMPARE(itemNames(repository.fetchLibraryItems(query)), QStringList({
            QStringLiteral("alpha.mp4"),
            QStringLiteral("bravo.mp4"),
            QStringLiteral("charlie.mp4"),
            QStringLiteral("wild%card.mp4"),
            QStringLiteral("wildXcard.mp4"),
        }));

        query.sortKey = MediaLibrarySortKey::Size;
        query.ascending = false;
        QCOMPARE(itemNames(repository.fetchLibraryItems(query)).first(), QStringLiteral("wildXcard.mp4"));

        query.sortKey = MediaLibrarySortKey::Modified;
        query.ascending = false;
        QCOMPARE(itemNames(repository.fetchLibraryItems(query)).first(), QStringLiteral("wildXcard.mp4"));

        query = {};
        query.searchText = QStringLiteral("target description");
        QCOMPARE(itemNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("alpha.mp4")}));

        query.searchText = QStringLiteral("wild%");
        QCOMPARE(itemNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("wild%card.mp4")}));

        QVERIFY(repository.lastError().isEmpty());
        testDatabase.database.close();
    }

    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M04LibraryQueryTest::modelExposesSelectionHelpersAndM04Roles()
{
    MediaLibraryModel model;
    QVector<MediaLibraryItem> items;

    MediaLibraryItem first;
    first.id = 10;
    first.fileName = QStringLiteral("first.mp4");
    first.fileSizeBytes = 123;
    first.durationMs = 456;
    first.thumbnailPath = QStringLiteral("C:/thumbs/first.jpg");
    items.append(first);

    MediaLibraryItem second;
    second.id = 20;
    second.fileName = QStringLiteral("second.mp4");
    items.append(second);

    model.setItems(items);

    QCOMPARE(model.mediaIdAt(0), 10);
    QCOMPARE(model.mediaIdAt(1), 20);
    QCOMPARE(model.mediaIdAt(2), -1);
    QCOMPARE(model.indexOfId(20), 1);
    QCOMPARE(model.indexOfId(99), -1);

    const QVariantMap row = model.get(0);
    QCOMPARE(row.value(QStringLiteral("fileSizeBytes")).toLongLong(), 123);
    QCOMPARE(row.value(QStringLiteral("durationMs")).toLongLong(), 456);
    QCOMPARE(row.value(QStringLiteral("thumbnailPath")).toString(), QStringLiteral("C:/thumbs/first.jpg"));
}

void M04LibraryQueryTest::controllerPreservesSelectedMediaAcrossRefreshes()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));

    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        QString insertError;
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("alpha.mp4"), 100, QStringLiteral("2026-04-01T00:00:00.000Z"), {}, {}, &insertError), qPrintable(insertError));
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("beta.mp4"), 300, QStringLiteral("2026-04-03T00:00:00.000Z"), {}, {}, &insertError), qPrintable(insertError));
        QVERIFY2(insertMedia(testDatabase.database, QStringLiteral("zeta.mp4"), 200, QStringLiteral("2026-04-02T00:00:00.000Z"), {}, {}, &insertError), qPrintable(insertError));

        MediaRepository repository(testDatabase.database);
        MediaLibraryModel model;
        model.setItems(repository.fetchLibraryItems());
        QVERIFY(repository.lastError().isEmpty());

        AppController controller(&model, &repository);
        controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());

        const int betaIndex = model.indexOfId(model.get(1).value(QStringLiteral("id")).toInt());
        controller.selectIndex(betaIndex);
        const int selectedId = controller.selectedMedia().value(QStringLiteral("id")).toInt();

        controller.setLibrarySortKey(QStringLiteral("size"));
        controller.setLibrarySortAscending(false);
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("id")).toInt(), selectedId);

        controller.setLibrarySearchText(QStringLiteral("zeta"));
        QCOMPARE(controller.selectedMedia().value(QStringLiteral("fileName")).toString(), QStringLiteral("zeta.mp4"));

        controller.setLibrarySearchText(QStringLiteral("does-not-exist"));
        QCOMPARE(controller.selectedIndex(), -1);
        QVERIFY(controller.selectedMedia().isEmpty());

        testDatabase.database.close();
    }

    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M04LibraryQueryTest::playbackControllerExposesLocalFileSource()
{
    PlaybackController controller;
    QVERIFY(!controller.hasSource());
    QVERIFY(controller.sourceUrl().isEmpty());

    const QString filePath = QDir::toNativeSeparators(QStringLiteral("C:/Media/video.mp4"));
    controller.setMedia({
        {QStringLiteral("filePath"), filePath},
    });

    QVERIFY(controller.hasSource());
    QCOMPARE(controller.sourceFilePath(), filePath);
    QCOMPARE(controller.sourceUrl(), QUrl::fromLocalFile(filePath));

    controller.clearSource();
    QVERIFY(!controller.hasSource());
    QVERIFY(controller.sourceUrl().isEmpty());
}

QTEST_MAIN(M04LibraryQueryTest)

#include "M04LibraryQueryTest.moc"

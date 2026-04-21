#include "app/AppController.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "TestSupport.h"

#include <QDir>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest/QtTest>

namespace {
using PickleTest::closeDatabase;
using PickleTest::createDatabase;
using PickleTest::initializeMigratedDatabase;
using PickleTest::TestDatabase;

struct SeedIds {
    int alpha = 0;
    int bravo = 0;
    int charlie = 0;
    int delta = 0;
    int echo = 0;
};

bool execPrepared(QSqlDatabase database, const QString &sql, const QVariantList &values, QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    if (errorMessage) errorMessage->clear();
    return true;
}

int insertMedia(
    QSqlDatabase database,
    const QString &fileName,
    qint64 fileSize,
    const QString &description,
    const QString &reviewStatus,
    bool favorite,
    bool deleteCandidate,
    const QString &lastPlayedAt,
    QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name, file_path, file_extension, file_size, modified_at, description,
            review_status, is_favorite, is_delete_candidate, last_played_at, last_position_ms
        )
        VALUES (?, ?, '.mp4', ?, '2026-04-20T00:00:00.000Z', ?, ?, ?, ?, ?, ?)
    )sql"));
    query.addBindValue(fileName);
    query.addBindValue(QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
    query.addBindValue(fileSize);
    query.addBindValue(description);
    query.addBindValue(reviewStatus);
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(deleteCandidate ? 1 : 0);
    query.addBindValue(lastPlayedAt);
    query.addBindValue(lastPlayedAt.isEmpty() ? 0 : 15000);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return 0;
    }
    if (errorMessage) errorMessage->clear();
    return query.lastInsertId().toInt();
}

bool addTag(QSqlDatabase database, int mediaId, const QString &tagName, QString *errorMessage = nullptr)
{
    if (!execPrepared(database, QStringLiteral("INSERT OR IGNORE INTO tags (name) VALUES (?)"), {tagName}, errorMessage)) {
        return false;
    }

    QSqlQuery selectQuery(database);
    selectQuery.prepare(QStringLiteral("SELECT id FROM tags WHERE name = ? COLLATE NOCASE LIMIT 1"));
    selectQuery.addBindValue(tagName);
    if (!selectQuery.exec() || !selectQuery.next()) {
        if (errorMessage) *errorMessage = selectQuery.lastError().text();
        return false;
    }

    return execPrepared(
        database,
        QStringLiteral("INSERT OR IGNORE INTO media_tags (media_id, tag_id) VALUES (?, ?)"),
        {mediaId, selectQuery.value(0).toInt()},
        errorMessage);
}

SeedIds seedSmartViewRows(QSqlDatabase database, QString *errorMessage = nullptr)
{
    SeedIds ids;
    ids.alpha = insertMedia(database, QStringLiteral("alpha.mp4"), 100, QStringLiteral("needle alpha"), QStringLiteral("unreviewed"), false, false, {}, errorMessage);
    ids.bravo = insertMedia(database, QStringLiteral("bravo.mp4"), 200, QStringLiteral("needle bravo"), QStringLiteral("reviewed"), true, false, QStringLiteral("2026-04-03T00:00:00.000Z"), errorMessage);
    ids.charlie = insertMedia(database, QStringLiteral("charlie.mp4"), 300, QStringLiteral("plain charlie"), QStringLiteral("needs_followup"), false, true, QStringLiteral("2026-04-01T00:00:00.000Z"), errorMessage);
    ids.delta = insertMedia(database, QStringLiteral("delta.mp4"), 400, QStringLiteral("needle delta"), QStringLiteral("unreviewed"), true, false, QStringLiteral("2026-04-02T00:00:00.000Z"), errorMessage);
    ids.echo = insertMedia(database, QStringLiteral("echo.mp4"), 500, QStringLiteral("plain echo"), QStringLiteral("reviewed"), false, false, {}, errorMessage);
    if (ids.alpha <= 0 || ids.bravo <= 0 || ids.charlie <= 0 || ids.delta <= 0 || ids.echo <= 0) {
        return ids;
    }
    addTag(database, ids.alpha, QStringLiteral("Blue"), errorMessage);
    addTag(database, ids.bravo, QStringLiteral("red"), errorMessage);
    addTag(database, ids.charlie, QStringLiteral("Blue"), errorMessage);
    addTag(database, ids.delta, QStringLiteral("Blue"), errorMessage);
    addTag(database, ids.delta, QStringLiteral("red"), errorMessage);
    addTag(database, ids.echo, QStringLiteral("green"), errorMessage);
    return ids;
}

QStringList libraryNames(const QVector<MediaLibraryItem> &items)
{
    QStringList names;
    for (const MediaLibraryItem &item : items) {
        names.append(item.fileName);
    }
    return names;
}

QStringList fileNames(const QVector<MediaFile> &items)
{
    QStringList names;
    for (const MediaFile &item : items) {
        names.append(item.fileName);
    }
    return names;
}

bool indexExists(QSqlDatabase database, const QString &indexName)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'index' AND name = ? LIMIT 1"));
    query.addBindValue(indexName);
    return query.exec() && query.next();
}

bool seedPopularTagRows(QSqlDatabase database, int mediaCount, int noiseTagCount, int noiseTagsPerMedia, QString *errorMessage = nullptr)
{
    if (!database.transaction()) {
        if (errorMessage) *errorMessage = database.lastError().text();
        return false;
    }

    QSqlQuery tagQuery(database);
    tagQuery.prepare(QStringLiteral("INSERT INTO tags (name) VALUES (?)"));
    tagQuery.bindValue(0, QStringLiteral("popular"));
    if (!tagQuery.exec()) {
        if (errorMessage) *errorMessage = tagQuery.lastError().text();
        database.rollback();
        return false;
    }
    const int popularTagId = tagQuery.lastInsertId().toInt();

    QVector<int> noiseTagIds;
    noiseTagIds.reserve(noiseTagCount);
    for (int index = 0; index < noiseTagCount; ++index) {
        tagQuery.bindValue(0, QStringLiteral("noise-%1").arg(index, 4, 10, QLatin1Char('0')));
        if (!tagQuery.exec()) {
            if (errorMessage) *errorMessage = tagQuery.lastError().text();
            database.rollback();
            return false;
        }
        noiseTagIds.append(tagQuery.lastInsertId().toInt());
    }

    QSqlQuery mediaQuery(database);
    mediaQuery.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name, file_path, file_extension, file_size, modified_at, description
        )
        VALUES (?, ?, '.mp4', ?, '2026-04-20T00:00:00.000Z', '')
    )sql"));

    QSqlQuery linkQuery(database);
    linkQuery.prepare(QStringLiteral("INSERT INTO media_tags (media_id, tag_id) VALUES (?, ?)"));

    for (int mediaIndex = 0; mediaIndex < mediaCount; ++mediaIndex) {
        const QString fileName = QStringLiteral("popular-%1.mp4").arg(mediaIndex, 5, 10, QLatin1Char('0'));
        mediaQuery.bindValue(0, fileName);
        mediaQuery.bindValue(1, QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
        mediaQuery.bindValue(2, 1000 + mediaIndex);
        if (!mediaQuery.exec()) {
            if (errorMessage) *errorMessage = mediaQuery.lastError().text();
            database.rollback();
            return false;
        }

        const int mediaId = mediaQuery.lastInsertId().toInt();
        linkQuery.bindValue(0, mediaId);
        linkQuery.bindValue(1, popularTagId);
        if (!linkQuery.exec()) {
            if (errorMessage) *errorMessage = linkQuery.lastError().text();
            database.rollback();
            return false;
        }

        for (int offset = 0; offset < noiseTagsPerMedia; ++offset) {
            linkQuery.bindValue(0, mediaId);
            linkQuery.bindValue(1, noiseTagIds.at((mediaIndex + offset) % noiseTagIds.size()));
            if (!linkQuery.exec()) {
                if (errorMessage) *errorMessage = linkQuery.lastError().text();
                database.rollback();
                return false;
            }
        }
    }

    if (!database.commit()) {
        if (errorMessage) *errorMessage = database.lastError().text();
        return false;
    }

    if (errorMessage) errorMessage->clear();
    return true;
}
}

class M22SmartViewsFilterTest : public QObject
{
    Q_OBJECT

private slots:
    void repositoryFiltersSmartViewsAndTags();
    void repositoryFiltersPopularTagsWithinDebugBudget();
    void migrationAddsSmartViewFilterIndexes();
    void controllerExposesAndAppliesLibraryFilters();
};

void M22SmartViewsFilterTest::repositoryFiltersSmartViewsAndTags()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m22-repository.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    seedSmartViewRows(testDatabase.database, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    MediaRepository repository(testDatabase.database);

    MediaLibraryQuery query;
    query.viewMode = MediaLibraryViewMode::Unreviewed;
    QCOMPARE(libraryNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("alpha.mp4"), QStringLiteral("delta.mp4")}));

    query = {};
    query.viewMode = MediaLibraryViewMode::Favorites;
    QCOMPARE(libraryNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("bravo.mp4"), QStringLiteral("delta.mp4")}));

    query = {};
    query.viewMode = MediaLibraryViewMode::DeleteCandidates;
    QCOMPARE(libraryNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("charlie.mp4")}));

    query = {};
    query.viewMode = MediaLibraryViewMode::Recent;
    query.sortKey = MediaLibrarySortKey::LastPlayed;
    query.ascending = false;
    QCOMPARE(libraryNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("bravo.mp4"), QStringLiteral("delta.mp4"), QStringLiteral("charlie.mp4")}));

    query = {};
    query.tagFilter = QStringLiteral("blue");
    QCOMPARE(libraryNames(repository.fetchLibraryItems(query)), QStringList({QStringLiteral("alpha.mp4"), QStringLiteral("charlie.mp4"), QStringLiteral("delta.mp4")}));

    query = {};
    query.searchText = QStringLiteral("needle");
    query.tagFilter = QStringLiteral("RED");
    query.viewMode = MediaLibraryViewMode::Favorites;
    query.sortKey = MediaLibrarySortKey::Size;
    query.ascending = false;
    QCOMPARE(fileNames(repository.fetchMediaFiles(query)), QStringList({QStringLiteral("delta.mp4"), QStringLiteral("bravo.mp4")}));

    const QStringList tags = repository.fetchTagNames();
    QVERIFY(tags.contains(QStringLiteral("Blue")));
    QVERIFY(tags.contains(QStringLiteral("red")));
    QVERIFY(repository.lastError().isEmpty());

    closeDatabase(&testDatabase);
}

void M22SmartViewsFilterTest::repositoryFiltersPopularTagsWithinDebugBudget()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m22-popular-tag.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    QVERIFY2(seedPopularTagRows(testDatabase.database, 12000, 240, 8, &error), qPrintable(error));

    MediaRepository repository(testDatabase.database);
    MediaLibraryQuery query;
    query.tagFilter = QStringLiteral("POPULAR");
    query.sortKey = MediaLibrarySortKey::Name;

    QElapsedTimer timer;
    timer.start();
    const QVector<MediaFile> files = repository.fetchMediaFiles(query);
    const qint64 elapsedMs = timer.elapsed();
    qInfo().noquote() << QStringLiteral("M22 popular tag fetch rows=%1 elapsed=%2ms").arg(files.size()).arg(elapsedMs);

    QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
    QCOMPARE(files.size(), 12000);
    QVERIFY2(elapsedMs < 10000, qPrintable(QStringLiteral("Popular tag filter took %1 ms").arg(elapsedMs)));

    closeDatabase(&testDatabase);
}

void M22SmartViewsFilterTest::migrationAddsSmartViewFilterIndexes()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m22-indexes.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));

    QVERIFY(indexExists(testDatabase.database, QStringLiteral("idx_media_files_review_status_name")));
    QVERIFY(indexExists(testDatabase.database, QStringLiteral("idx_media_files_favorite_name")));
    QVERIFY(indexExists(testDatabase.database, QStringLiteral("idx_media_files_delete_candidate_name")));
    QVERIFY(indexExists(testDatabase.database, QStringLiteral("idx_media_files_last_played_name")));

    closeDatabase(&testDatabase);
}

void M22SmartViewsFilterTest::controllerExposesAndAppliesLibraryFilters()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m22-controller.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    const SeedIds ids = seedSmartViewRows(testDatabase.database, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    MediaRepository repository(testDatabase.database);
    MediaLibraryModel model;
    model.setItems(repository.fetchLibraryItems());

    AppController controller(&model, &repository);
    const QMetaObject &metaObject = AppController::staticMetaObject;
    QVERIFY(metaObject.indexOfProperty("libraryViewMode") >= 0);
    QVERIFY(metaObject.indexOfProperty("libraryTagFilter") >= 0);
    QVERIFY(metaObject.indexOfProperty("availableTags") >= 0);

    controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());
    QVERIFY(controller.availableTags().contains(QStringLiteral("Blue")));
    QVERIFY(controller.availableTags().contains(QStringLiteral("red")));

    controller.selectIndex(model.indexOfId(ids.bravo));
    QCOMPARE(controller.selectedMedia().value(QStringLiteral("fileName")).toString(), QStringLiteral("bravo.mp4"));

    controller.setLibraryViewMode(QStringLiteral("favorites"));
    QTRY_COMPARE(model.rowCount(), 2);
    QCOMPARE(controller.selectedMedia().value(QStringLiteral("fileName")).toString(), QStringLiteral("bravo.mp4"));

    controller.setLibraryTagFilter(QStringLiteral("blue"));
    QTRY_COMPARE(model.rowCount(), 1);
    QCOMPARE(model.get(0).value(QStringLiteral("fileName")).toString(), QStringLiteral("delta.mp4"));
    QCOMPARE(controller.selectedMedia().value(QStringLiteral("fileName")).toString(), QStringLiteral("delta.mp4"));

    controller.setLibraryTagFilter(QString());
    controller.setLibraryViewMode(QStringLiteral("recent"));
    QTRY_COMPARE(model.rowCount(), 3);
    QCOMPARE(controller.librarySortKey(), QStringLiteral("lastPlayed"));
    QCOMPARE(controller.librarySortAscending(), false);
    QCOMPARE(model.get(0).value(QStringLiteral("fileName")).toString(), QStringLiteral("bravo.mp4"));

    controller.setLibraryViewMode(QStringLiteral("favorites"));
    QTRY_COMPARE(model.rowCount(), 2);
    controller.selectIndex(model.indexOfId(ids.delta));
    controller.setSelectedFavorite(false);
    QTRY_COMPARE(model.rowCount(), 1);
    QCOMPARE(model.get(0).value(QStringLiteral("fileName")).toString(), QStringLiteral("bravo.mp4"));

    closeDatabase(&testDatabase);
}

QTEST_MAIN(M22SmartViewsFilterTest)

#include "M22SmartViewsFilterTest.moc"

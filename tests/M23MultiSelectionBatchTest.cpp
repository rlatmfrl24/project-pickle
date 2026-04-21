#include "app/AppController.h"
#include "application/BatchUpdateMediaUseCase.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "TestSupport.h"

#include <QDir>
#include <QFile>
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

QString sourcePath(const QString &relativePath)
{
    return QStringLiteral(PICKLE_SOURCE_DIR) + QLatin1Char('/') + relativePath;
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

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

int insertMedia(QSqlDatabase database, const QString &fileName, QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (file_name, file_path, file_extension, file_size, modified_at)
        VALUES (?, ?, '.mp4', 1000, '2026-04-20T00:00:00.000Z')
    )sql"));
    query.addBindValue(fileName);
    query.addBindValue(QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
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
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT id FROM tags WHERE name = ? COLLATE NOCASE LIMIT 1"));
    query.addBindValue(tagName);
    if (!query.exec() || !query.next()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    return execPrepared(database, QStringLiteral("INSERT OR IGNORE INTO media_tags (media_id, tag_id) VALUES (?, ?)"), {mediaId, query.value(0)}, errorMessage);
}

int countRowsWhere(QSqlDatabase database, const QString &whereClause)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM media_files WHERE %1").arg(whereClause)) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

QVector<int> seedRows(QSqlDatabase database, QString *errorMessage = nullptr)
{
    QVector<int> ids;
    for (const QString &name : {QStringLiteral("alpha.mp4"), QStringLiteral("bravo.mp4"), QStringLiteral("charlie.mp4"), QStringLiteral("delta.mp4")}) {
        ids.append(insertMedia(database, name, errorMessage));
    }
    if (ids.size() == 4 && ids.at(0) > 0) {
        addTag(database, ids.at(0), QStringLiteral("keep"), errorMessage);
        addTag(database, ids.at(1), QStringLiteral("keep"), errorMessage);
        addTag(database, ids.at(2), QStringLiteral("other"), errorMessage);
    }
    return ids;
}

QVariantList oneTag(const QString &tag)
{
    return QVariantList({tag});
}
}

class M23MultiSelectionBatchTest : public QObject
{
    Q_OBJECT

private slots:
    void repositoryAndUseCaseApplyBatchUpdates();
    void controllerMaintainsVisibleSelectionAndRunsBatchTags();
    void qmlFacingBatchUiIsWired();
};

void M23MultiSelectionBatchTest::repositoryAndUseCaseApplyBatchUpdates()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());
    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m23-repository.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    const QVector<int> ids = seedRows(testDatabase.database, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    MediaRepository repository(testDatabase.database);
    BatchUpdateMediaUseCase useCase(&repository);

    QVERIFY2(useCase.addTags(ids, {QStringLiteral("Batch"), QStringLiteral("batch"), QString()}).succeeded, qPrintable(repository.lastError()));
    QCOMPARE(repository.fetchMediaFileById(ids.at(0)).tags.count(QStringLiteral("Batch")), 1);
    QVERIFY2(useCase.removeTags({ids.at(1), ids.at(2)}, {QStringLiteral("missing")}).succeeded, qPrintable(repository.lastError()));
    QVERIFY2(useCase.removeTags({ids.at(1), ids.at(2)}, {QStringLiteral("BATCH")}).succeeded, qPrintable(repository.lastError()));
    QVERIFY(repository.fetchMediaFileById(ids.at(1)).tags.indexOf(QStringLiteral("Batch")) < 0);
    QVERIFY(repository.fetchMediaFileById(ids.at(0)).tags.contains(QStringLiteral("Batch")));

    QVERIFY(useCase.setReviewStatus({ids.at(0), ids.at(1)}, QStringLiteral("reviewed")).succeeded);
    QCOMPARE(repository.fetchMediaFileById(ids.at(1)).reviewStatus, QStringLiteral("reviewed"));
    QVERIFY(!useCase.setReviewStatus({ids.at(0)}, QStringLiteral("bad")).succeeded);

    QVERIFY(useCase.setRating(ids, 0).succeeded);
    QCOMPARE(repository.fetchMediaFileById(ids.at(2)).rating, 0);
    QVERIFY(!useCase.setRating(ids, 6).succeeded);

    QVector<int> manyIds;
    QVERIFY(testDatabase.database.transaction());
    for (int index = 0; index < 905; ++index) {
        manyIds.append(insertMedia(testDatabase.database, QStringLiteral("bulk-%1.mp4").arg(index, 4, 10, QLatin1Char('0')), &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    QVERIFY(testDatabase.database.commit());
    QVERIFY(useCase.setRating(manyIds, 5).succeeded);
    QCOMPARE(countRowsWhere(testDatabase.database, QStringLiteral("rating = 5")), 905);

    closeDatabase(&testDatabase);
}

void M23MultiSelectionBatchTest::controllerMaintainsVisibleSelectionAndRunsBatchTags()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());
    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m23-controller.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    const QVector<int> ids = seedRows(testDatabase.database, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    MediaRepository repository(testDatabase.database);
    MediaLibraryModel model;
    model.setItems(repository.fetchLibraryItems());
    AppController controller(&model, &repository);
    controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());

    QCOMPARE(controller.selectedMediaCount(), 1);
    QVERIFY(model.isSelected(0));
    controller.selectRangeOrToggle(2, false, true);
    QCOMPARE(controller.selectedMediaCount(), 3);
    QCOMPARE(controller.selectedIndex(), 2);
    QVERIFY(model.isSelected(0));
    QVERIFY(model.isSelected(1));
    QVERIFY(model.isSelected(2));

    controller.selectRangeOrToggle(1, true, false);
    QCOMPARE(controller.selectedMediaCount(), 2);
    QVERIFY(!model.isSelected(1));

    controller.setLibraryTagFilter(QStringLiteral("keep"));
    QTRY_COMPARE(model.rowCount(), 2);
    QTRY_COMPARE(controller.selectedMediaCount(), 1);
    QCOMPARE(controller.selectedMedia().value(QStringLiteral("fileName")).toString(), QStringLiteral("alpha.mp4"));

    controller.selectAllVisible();
    QCOMPARE(controller.selectedMediaCount(), 2);
    controller.addTagsToSelectedMedia(oneTag(QStringLiteral("Batch")));
    QTRY_VERIFY(controller.detailStatus().startsWith(QStringLiteral("Tags added")));
    QVERIFY(repository.fetchMediaFileById(ids.at(0)).tags.contains(QStringLiteral("Batch")));
    QVERIFY(repository.fetchMediaFileById(ids.at(1)).tags.contains(QStringLiteral("Batch")));
    QVERIFY(controller.availableTags().contains(QStringLiteral("Batch")));

    closeDatabase(&testDatabase);
}

void M23MultiSelectionBatchTest::qmlFacingBatchUiIsWired()
{
    const QString libraryPanel = readTextFile(sourcePath(QStringLiteral("qml/components/LibraryPanel.qml")));
    const QString infoPanel = readTextFile(sourcePath(QStringLiteral("qml/components/InfoPanel.qml")));
    QVERIFY2(!libraryPanel.isEmpty(), "Could not read LibraryPanel.qml");
    QVERIFY2(!infoPanel.isEmpty(), "Could not read InfoPanel.qml");

    QVERIFY(libraryPanel.contains(QStringLiteral("signal selectionRequested")));
    QVERIFY(libraryPanel.contains(QStringLiteral("required property bool isSelected")));
    QVERIFY(libraryPanel.contains(QStringLiteral("Qt.ControlModifier")));
    QVERIFY(libraryPanel.contains(QStringLiteral("Qt.ShiftModifier")));
    QVERIFY(infoPanel.contains(QStringLiteral("property int selectedMediaCount")));
    QVERIFY(infoPanel.contains(QStringLiteral("readonly property bool batchMode")));
    QVERIFY(infoPanel.contains(QStringLiteral("addTagsToSelectedMedia")));
    QVERIFY(infoPanel.contains(QStringLiteral("removeTagsFromSelectedMedia")));
    QVERIFY(infoPanel.contains(QStringLiteral("setSelectedMediaReviewStatus")));
    QVERIFY(infoPanel.contains(QStringLiteral("setSelectedMediaRating")));
}

QTEST_MAIN(M23MultiSelectionBatchTest)

#include "M23MultiSelectionBatchTest.moc"

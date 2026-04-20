#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/MediaLibraryModel.h"
#include "media/SnapshotService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
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
    result.connectionName = QStringLiteral("m08_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    result.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), result.connectionName);
    result.database.setDatabaseName(databasePath);
    return result;
}

bool writeFile(const QString &filePath, const QByteArray &contents = QByteArray("pickle"))
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(contents);
    return true;
}

int insertMedia(QSqlDatabase database, const QString &filePath, QString *errorMessage = nullptr)
{
    QFileInfo fileInfo(filePath);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name,
            file_path,
            file_extension,
            file_size,
            modified_at
        )
        VALUES (?, ?, ?, ?, ?)
    )sql"));
    query.addBindValue(fileInfo.fileName());
    query.addBindValue(QDir::toNativeSeparators(fileInfo.absoluteFilePath()));
    query.addBindValue(QStringLiteral(".%1").arg(fileInfo.suffix().toLower()));
    query.addBindValue(fileInfo.exists() ? fileInfo.size() : 1024);
    query.addBindValue(QStringLiteral("2026-04-20T00:00:00.000Z"));

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

int rowCount(QSqlDatabase database, const QString &tableName)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName)) || !query.next()) {
        return -1;
    }

    return query.value(0).toInt();
}
}

class M08SnapshotThumbnailTest : public QObject
{
    Q_OBJECT

private slots:
    void repositoryStoresSnapshotsAndThumbnail();
    void modelUpdatesThumbnailRole();
    void snapshotServiceRejectsInvalidInputAndClearsRoot();
};

void M08SnapshotThumbnailTest::repositoryStoresSnapshotsAndThumbnail()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));
    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        const QString mediaPath = databaseDir.filePath(QStringLiteral("movie.mp4"));
        const QString firstSnapshotPath = databaseDir.filePath(QStringLiteral("first.jpg"));
        const QString secondSnapshotPath = databaseDir.filePath(QStringLiteral("second.jpg"));
        QVERIFY(writeFile(mediaPath));
        QVERIFY(writeFile(firstSnapshotPath, QByteArray("jpg1")));
        QVERIFY(writeFile(secondSnapshotPath, QByteArray("jpg2")));

        QString insertError;
        const int mediaId = insertMedia(testDatabase.database, mediaPath, &insertError);
        QVERIFY2(mediaId > 0, qPrintable(insertError));

        MediaRepository repository(testDatabase.database);
        int firstSnapshotId = -1;
        int secondSnapshotId = -1;
        QVERIFY2(repository.addSnapshot(mediaId, QDir::toNativeSeparators(firstSnapshotPath), 1000, &firstSnapshotId), qPrintable(repository.lastError()));
        QVERIFY(firstSnapshotId > 0);
        QVERIFY2(repository.addSnapshot(mediaId, QDir::toNativeSeparators(secondSnapshotPath), 2000, &secondSnapshotId), qPrintable(repository.lastError()));
        QVERIFY(secondSnapshotId > firstSnapshotId);

        QVector<SnapshotItem> snapshots = repository.fetchSnapshotsForMedia(mediaId);
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(snapshots.size(), 2);
        QCOMPARE(snapshots.first().id, secondSnapshotId);
        QCOMPARE(snapshots.first().timestampMs, 2000);
        QCOMPARE(snapshots.last().id, firstSnapshotId);

        QVERIFY2(repository.setMediaThumbnailPath(mediaId, QDir::toNativeSeparators(secondSnapshotPath)), qPrintable(repository.lastError()));
        const QVector<MediaLibraryItem> items = repository.fetchLibraryItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().thumbnailPath, QString());

        QVERIFY2(repository.resetLibraryData(), qPrintable(repository.lastError()));
        QCOMPARE(rowCount(testDatabase.database, QStringLiteral("snapshots")), 0);

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M08SnapshotThumbnailTest::modelUpdatesThumbnailRole()
{
    MediaLibraryModel model;
    MediaLibraryItem item;
    item.id = 42;
    item.fileName = QStringLiteral("thumbnail.mp4");
    item.thumbnailPath = QStringLiteral("old.jpg");
    model.setItems({item});

    QSignalSpy dataChangedSpy(&model, &MediaLibraryModel::dataChanged);
    QVERIFY(model.setThumbnailPath(42, QStringLiteral("new.jpg")));
    QCOMPARE(model.get(0).value(QStringLiteral("thumbnailPath")).toString(), QStringLiteral("new.jpg"));
    QCOMPARE(dataChangedSpy.size(), 1);

    const QList<QVariant> signalArguments = dataChangedSpy.takeFirst();
    const QList<int> roles = signalArguments.at(2).value<QList<int>>();
    QCOMPARE(roles, QList<int>({MediaLibraryModel::ThumbnailPathRole}));
}

void M08SnapshotThumbnailTest::snapshotServiceRejectsInvalidInputAndClearsRoot()
{
    SnapshotService service;

    SnapshotCaptureRequest missingFileRequest;
    missingFileRequest.mediaId = 1;
    missingFileRequest.filePath = QStringLiteral("C:/missing/video.mp4");
    missingFileRequest.outputRootPath = SnapshotService::defaultSnapshotRoot();
    missingFileRequest.timestampMs = 1000;

    SnapshotCaptureResult result = service.capture(missingFileRequest);
    QVERIFY(!result.succeeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("File does not exist")));

    SnapshotCaptureRequest emptyRootRequest;
    QTemporaryDir mediaDir;
    QVERIFY(mediaDir.isValid());
    const QString mediaPath = mediaDir.filePath(QStringLiteral("video.mp4"));
    QVERIFY(writeFile(mediaPath));
    emptyRootRequest.mediaId = 1;
    emptyRootRequest.filePath = mediaPath;
    emptyRootRequest.outputRootPath = QString();
    result = service.capture(emptyRootRequest);
    QVERIFY(!result.succeeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("output folder")));

    QStandardPaths::setTestModeEnabled(true);
    const QString rootPath = SnapshotService::defaultSnapshotRoot();
    const QString nestedPath = QDir(rootPath).filePath(QStringLiteral("1"));
    QVERIFY(QDir().mkpath(nestedPath));
    QVERIFY(writeFile(QDir(nestedPath).filePath(QStringLiteral("snapshot.jpg")), QByteArray("jpg")));
    QVERIFY(QDir(rootPath).exists());

    QString clearError;
    QVERIFY2(SnapshotService::clearSnapshotRoot(rootPath, &clearError), qPrintable(clearError));
    QVERIFY(!QDir(rootPath).exists());

    QTemporaryDir outsideDir;
    QVERIFY(outsideDir.isValid());
    const QString outsideRoot = outsideDir.filePath(QStringLiteral("snapshots"));
    QVERIFY(QDir().mkpath(outsideRoot));
    QVERIFY(!SnapshotService::clearSnapshotRoot(outsideRoot, &clearError));
    QVERIFY(clearError.contains(QStringLiteral("outside")));
    QVERIFY(QDir(outsideRoot).exists());
}

QTEST_MAIN(M08SnapshotThumbnailTest)

#include "M08SnapshotThumbnailTest.moc"

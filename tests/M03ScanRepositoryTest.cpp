#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/ScanService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace {
bool writeFile(const QString &filePath, const QByteArray &content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    return file.write(content) == content.size();
}

int mediaFileCount(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM media_files")) || !query.next()) {
        return -1;
    }

    return query.value(0).toInt();
}
}

class M03ScanRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void scansVideoFilesAndUpsertsByPath();
};

void M03ScanRepositoryTest::scansVideoFilesAndUpsertsByPath()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    QTemporaryDir mediaDir;
    QVERIFY(mediaDir.isValid());

    QDir root(mediaDir.path());
    QVERIFY(root.mkpath(QStringLiteral("sub")));

    const QString mp4Path = root.filePath(QStringLiteral("a.mp4"));
    const QString mkvPath = root.filePath(QStringLiteral("sub/b.mkv"));
    const QString textPath = root.filePath(QStringLiteral("ignore.txt"));
    QVERIFY(writeFile(mp4Path, QByteArray("mp4")));
    QVERIFY(writeFile(mkvPath, QByteArray("mkv")));
    QVERIFY(writeFile(textPath, QByteArray("txt")));

    const QString connectionName = QStringLiteral("m03_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databaseDir.filePath(QStringLiteral("test.sqlite3")));
        QVERIFY(database.open());

        QSqlQuery pragmaQuery(database);
        QVERIFY(pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON")));

        MigrationService migrationService(database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        ScanService scanService;
        DirectoryScanResult scanResult = scanService.scanDirectory(mediaDir.path());
        QVERIFY2(scanResult.succeeded, qPrintable(scanResult.errorMessage));
        QCOMPARE(scanResult.files.size(), 2);

        MediaRepository repository(database);
        MediaUpsertResult upsertResult;
        QVERIFY2(repository.upsertScanResult(scanResult.rootPath, scanResult.files, &upsertResult), qPrintable(repository.lastError()));
        QCOMPARE(upsertResult.scannedFileCount, 2);
        QCOMPARE(upsertResult.upsertedFileCount, 2);
        QCOMPARE(mediaFileCount(database), 2);

        const QVector<MediaLibraryItem> items = repository.fetchLibraryItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), 2);

        QSet<QString> fileNames;
        for (const MediaLibraryItem &item : items) {
            fileNames.insert(item.fileName);
        }
        QVERIFY(fileNames.contains(QStringLiteral("a.mp4")));
        QVERIFY(fileNames.contains(QStringLiteral("b.mkv")));
        QVERIFY(!fileNames.contains(QStringLiteral("ignore.txt")));

        QVERIFY(writeFile(mp4Path, QByteArray("mp4-larger")));

        DirectoryScanResult secondScanResult = scanService.scanDirectory(mediaDir.path());
        QVERIFY2(secondScanResult.succeeded, qPrintable(secondScanResult.errorMessage));
        QVERIFY2(repository.upsertScanResult(secondScanResult.rootPath, secondScanResult.files, &upsertResult), qPrintable(repository.lastError()));
        QCOMPARE(mediaFileCount(database), 2);

        QSqlQuery sizeQuery(database);
        sizeQuery.prepare(QStringLiteral("SELECT file_size FROM media_files WHERE file_name = ?"));
        sizeQuery.addBindValue(QStringLiteral("a.mp4"));
        QVERIFY(sizeQuery.exec());
        QVERIFY(sizeQuery.next());
        QCOMPARE(sizeQuery.value(0).toLongLong(), QFileInfo(mp4Path).size());

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(M03ScanRepositoryTest)

#include "M03ScanRepositoryTest.moc"

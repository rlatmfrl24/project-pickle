#include "core/CancellationToken.h"
#include "core/ProcessRunner.h"
#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/ScanService.h"
#include "media/ThumbnailService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <memory>

namespace {
struct TestDatabase {
    QSqlDatabase database;
    QString connectionName;
};

TestDatabase createDatabase(const QString &databasePath)
{
    TestDatabase result;
    result.connectionName = QStringLiteral("m09_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
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
    const QFileInfo fileInfo(filePath);
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
}

class M09QualityPerformanceTest : public QObject
{
    Q_OBJECT

private slots:
    void scanCanBeCanceledBeforeCollectingFiles();
    void processRunnerHandlesCancelAndTimeout();
    void thumbnailServiceCreatesSmallCacheAndRejectsMissingSource();
    void repositoryBackfillPrefersLatestSnapshotAndKeepsCachedThumbnailSeparate();
};

void M09QualityPerformanceTest::scanCanBeCanceledBeforeCollectingFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(writeFile(tempDir.filePath(QStringLiteral("movie.mp4"))));

    auto cancellation = std::make_shared<CancellationToken>();
    cancellation->cancel();

    ScanService scanService;
    bool progressReported = false;
    const DirectoryScanResult result = scanService.scanDirectory(
        tempDir.path(),
        cancellation,
        [&progressReported](const ScanProgress &progress) {
            progressReported = true;
            QVERIFY(progress.canceled);
        });

    QVERIFY(result.canceled);
    QVERIFY(!result.succeeded);
    QVERIFY(result.files.isEmpty());
    QVERIFY(progressReported);
}

void M09QualityPerformanceTest::processRunnerHandlesCancelAndTimeout()
{
    ProcessRunner runner;

    auto cancellation = std::make_shared<CancellationToken>();
    cancellation->cancel();
    const ProcessResult canceled = runner.run(QStringLiteral("cmd"), {QStringLiteral("/c"), QStringLiteral("echo canceled")}, 1000, cancellation);
    QVERIFY(canceled.canceled);
    QVERIFY(!canceled.succeeded);

    const ProcessResult timedOut = runner.run(
        QStringLiteral("ping"),
        {QStringLiteral("127.0.0.1"), QStringLiteral("-n"), QStringLiteral("6")},
        100);
    QVERIFY(timedOut.timedOut);
    QVERIFY(!timedOut.succeeded);
}

void M09QualityPerformanceTest::thumbnailServiceCreatesSmallCacheAndRejectsMissingSource()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString sourcePath = tempDir.filePath(QStringLiteral("source.jpg"));
    QImage sourceImage(800, 400, QImage::Format_RGB32);
    sourceImage.fill(Qt::red);
    QVERIFY(sourceImage.save(sourcePath, "JPG"));

    ThumbnailGenerationRequest request;
    request.mediaId = 7;
    request.sourceImagePath = sourcePath;
    request.outputRootPath = tempDir.filePath(QStringLiteral("thumbnails"));
    request.maxWidth = 320;
    request.quality = 82;

    ThumbnailService service;
    const ThumbnailGenerationResult result = service.generate(request);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QVERIFY(QFileInfo::exists(result.thumbnailPath));

    {
        QImageReader reader(result.thumbnailPath);
        const QSize generatedSize = reader.size();
        QVERIFY(generatedSize.isValid());
        QVERIFY(generatedSize.width() <= 320);
    }
    QVERIFY(ThumbnailService::isUsableThumbnail(result.thumbnailPath, 320));

    request.sourceImagePath = tempDir.filePath(QStringLiteral("missing.jpg"));
    const ThumbnailGenerationResult missingResult = service.generate(request);
    QVERIFY(!missingResult.succeeded);
    QVERIFY(missingResult.errorMessage.contains(QStringLiteral("does not exist")));

    QString clearError;
    QVERIFY2(ThumbnailService::clearThumbnailRoot(tempDir.filePath(QStringLiteral("thumbnails")), &clearError), qPrintable(clearError));
}

void M09QualityPerformanceTest::repositoryBackfillPrefersLatestSnapshotAndKeepsCachedThumbnailSeparate()
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
        const QString cachedThumbnailPath = databaseDir.filePath(QStringLiteral("thumb_w320.jpg"));
        QVERIFY(writeFile(mediaPath));
        QVERIFY(writeFile(firstSnapshotPath, QByteArray("jpg1")));
        QVERIFY(writeFile(secondSnapshotPath, QByteArray("jpg2")));
        QVERIFY(writeFile(cachedThumbnailPath, QByteArray("thumb")));

        QString insertError;
        const int mediaId = insertMedia(testDatabase.database, mediaPath, &insertError);
        QVERIFY2(mediaId > 0, qPrintable(insertError));

        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.addSnapshot(mediaId, QDir::toNativeSeparators(firstSnapshotPath), 1000), qPrintable(repository.lastError()));
        QVERIFY2(repository.addSnapshot(mediaId, QDir::toNativeSeparators(secondSnapshotPath), 2000), qPrintable(repository.lastError()));
        QVERIFY2(repository.setMediaThumbnailPath(mediaId, QDir::toNativeSeparators(cachedThumbnailPath)), qPrintable(repository.lastError()));

        const QVector<ThumbnailBackfillItem> items = repository.fetchThumbnailBackfillItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().mediaId, mediaId);
        QCOMPARE(items.first().sourceImagePath, QDir::toNativeSeparators(secondSnapshotPath));
        QCOMPARE(items.first().existingThumbnailPath, QDir::toNativeSeparators(cachedThumbnailPath));

        testDatabase.database.close();
    }
    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

QTEST_MAIN(M09QualityPerformanceTest)

#include "M09QualityPerformanceTest.moc"

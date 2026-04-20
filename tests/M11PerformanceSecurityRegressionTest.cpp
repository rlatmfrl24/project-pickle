#include "app/AppController.h"
#include "core/ExternalToolService.h"
#include "core/ProcessRunner.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"
#include "TestSupport.h"

#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {
using PickleTest::closeDatabase;
using PickleTest::createDatabase;
using PickleTest::initializeMigratedDatabase;
using PickleTest::TestDatabase;
using PickleTest::writeFile;

bool execSql(QSqlDatabase database, const QString &sql, QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
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

bool seedLargeTaggedLibrary(QSqlDatabase database, int mediaCount, int tagCount, int tagsPerMedia, QString *errorMessage = nullptr)
{
    if (!database.transaction()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    QVector<int> tagIds;
    tagIds.reserve(tagCount);
    QSqlQuery tagQuery(database);
    tagQuery.prepare(QStringLiteral("INSERT INTO tags (name) VALUES (?)"));
    for (int tagIndex = 0; tagIndex < tagCount; ++tagIndex) {
        tagQuery.bindValue(0, QStringLiteral("tag-%1").arg(tagIndex));
        if (!tagQuery.exec()) {
            if (errorMessage) {
                *errorMessage = tagQuery.lastError().text();
            }
            database.rollback();
            return false;
        }
        tagIds.append(tagQuery.lastInsertId().toInt());
    }

    QSqlQuery mediaQuery(database);
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

    QSqlQuery linkQuery(database);
    linkQuery.prepare(QStringLiteral("INSERT INTO media_tags (media_id, tag_id) VALUES (?, ?)"));

    for (int index = 0; index < mediaCount; ++index) {
        const QString fileName = QStringLiteral("video-%1.mp4").arg(index, 5, 10, QLatin1Char('0'));
        mediaQuery.bindValue(0, fileName);
        mediaQuery.bindValue(1, QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
        mediaQuery.bindValue(2, QStringLiteral(".mp4"));
        mediaQuery.bindValue(3, 1024 + index);
        mediaQuery.bindValue(4, QStringLiteral("2026-04-20T00:00:00.000Z"));
        mediaQuery.bindValue(5, index % 1000 == 42 ? QStringLiteral("needle-%1").arg(index) : QStringLiteral(""));
        if (!mediaQuery.exec()) {
            if (errorMessage) {
                *errorMessage = mediaQuery.lastError().text();
            }
            database.rollback();
            return false;
        }

        const int mediaId = mediaQuery.lastInsertId().toInt();
        for (int offset = 0; offset < tagsPerMedia; ++offset) {
            linkQuery.bindValue(0, mediaId);
            linkQuery.bindValue(1, tagIds.at((index + offset) % tagIds.size()));
            if (!linkQuery.exec()) {
                if (errorMessage) {
                    *errorMessage = linkQuery.lastError().text();
                }
                database.rollback();
                return false;
            }
        }
    }

    if (!database.commit()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

int countRowsWhere(QSqlDatabase database, const QString &whereClause, QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM media_files WHERE %1").arg(whereClause)) || !query.next()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return query.value(0).toInt();
}
}

class M11PerformanceSecurityRegressionTest : public QObject
{
    Q_OBJECT

private slots:
    void repositoryFetchesFiftyThousandTaggedRowsWithinDebugBudget();
    void unchangedScanDoesNotRewriteRows();
    void librarySearchIsDebouncedAndAsync();
    void modelUsesIndexedLookupAndTightSignals();
    void localSecurityHardeningRejectsUnsafeInputs();
};

void M11PerformanceSecurityRegressionTest::repositoryFetchesFiftyThousandTaggedRowsWithinDebugBudget()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("large.sqlite3")));
    QString databaseError;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &databaseError), qPrintable(databaseError));

    QString error;
    constexpr int MediaCount = 50000;
    QVERIFY2(seedLargeTaggedLibrary(testDatabase.database, MediaCount, 12, 3, &error), qPrintable(error));

    {
        MediaRepository repository(testDatabase.database);
        const QVector<MediaLibrarySortKey> sortKeys = {
            MediaLibrarySortKey::Name,
            MediaLibrarySortKey::Size,
            MediaLibrarySortKey::Modified,
        };

        for (const MediaLibrarySortKey sortKey : sortKeys) {
            MediaLibraryQuery query;
            query.sortKey = sortKey;
            query.ascending = sortKey != MediaLibrarySortKey::Size;

            QElapsedTimer timer;
            timer.start();
            const QVector<MediaLibraryItem> items = repository.fetchLibraryItems(query);
            const qint64 elapsedMs = timer.elapsed();
            qInfo().noquote() << QStringLiteral("M11 full fetch sort=%1 rows=%2 elapsed=%3ms")
                .arg(static_cast<int>(sortKey))
                .arg(items.size())
                .arg(elapsedMs);

            QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
            QCOMPARE(items.size(), MediaCount);
            QCOMPARE(items.first().tags.size(), 3);
            QVERIFY2(elapsedMs < 30000, qPrintable(QStringLiteral("Fetching %1 rows took %2 ms").arg(MediaCount).arg(elapsedMs)));
        }
    }

    closeDatabase(&testDatabase);
}

void M11PerformanceSecurityRegressionTest::unchangedScanDoesNotRewriteRows()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("scan.sqlite3")));
    QString databaseError;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &databaseError), qPrintable(databaseError));

    constexpr int MediaCount = 50000;
    QVector<ScannedMediaFile> files;
    files.reserve(MediaCount);
    for (int index = 0; index < MediaCount; ++index) {
        ScannedMediaFile file;
        file.fileName = QStringLiteral("video-%1.mp4").arg(index, 5, 10, QLatin1Char('0'));
        file.filePath = QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(file.fileName));
        file.fileExtension = QStringLiteral(".mp4");
        file.fileSize = 1024 + index;
        file.modifiedAt = QStringLiteral("2026-04-20T00:00:00.000Z");
        files.append(file);
    }

    MediaUpsertResult result;
    {
        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.upsertScanResult(QStringLiteral("C:/Media"), files, &result), qPrintable(repository.lastError()));
        QCOMPARE(result.upsertedFileCount, MediaCount);
    }

    QString error;
    QVERIFY2(execSql(testDatabase.database, QStringLiteral("UPDATE media_files SET updated_at = 'unchanged-marker'"), &error), qPrintable(error));

    QElapsedTimer timer;
    timer.start();
    {
        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.upsertScanResult(QStringLiteral("C:/Media"), files, &result), qPrintable(repository.lastError()));
    }
    const qint64 elapsedMs = timer.elapsed();
    qInfo().noquote() << QStringLiteral("M11 unchanged rescan rows=%1 elapsed=%2ms").arg(MediaCount).arg(elapsedMs);

    QCOMPARE(result.upsertedFileCount, MediaCount);
    QCOMPARE(countRowsWhere(testDatabase.database, QStringLiteral("updated_at <> 'unchanged-marker'"), &error), 0);
    QVERIFY2(elapsedMs < 30000, qPrintable(QStringLiteral("Unchanged rescan took %1 ms").arg(elapsedMs)));

    closeDatabase(&testDatabase);
}

void M11PerformanceSecurityRegressionTest::librarySearchIsDebouncedAndAsync()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("debounce.sqlite3")));
    QString databaseError;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &databaseError), qPrintable(databaseError));

    QVector<ScannedMediaFile> files;
    for (const QString &name : {QStringLiteral("alpha.mp4"), QStringLiteral("beta.mp4"), QStringLiteral("gamma.mp4")}) {
        ScannedMediaFile file;
        file.fileName = name;
        file.filePath = QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(name));
        file.fileExtension = QStringLiteral(".mp4");
        file.fileSize = 1024;
        file.modifiedAt = QStringLiteral("2026-04-20T00:00:00.000Z");
        files.append(file);
    }

    MediaLibraryModel model;
    {
        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.upsertScanResult(QStringLiteral("C:/Media"), files), qPrintable(repository.lastError()));
        model.setItems(repository.fetchLibraryItems());
    }
    QCOMPARE(model.rowCount(), 3);

    {
        MediaRepository repository(testDatabase.database);
        AppController controller(&model, &repository);
        controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        controller.setLibrarySearchText(QStringLiteral("a"));
        controller.setLibrarySearchText(QStringLiteral("al"));
        controller.setLibrarySearchText(QStringLiteral("alpha"));

        QTest::qWait(80);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(resetSpy.size(), 0);

        QTRY_COMPARE(model.rowCount(), 1);
        QCOMPARE(model.get(0).value(QStringLiteral("fileName")).toString(), QStringLiteral("alpha.mp4"));
    }

    closeDatabase(&testDatabase);
}

void M11PerformanceSecurityRegressionTest::modelUsesIndexedLookupAndTightSignals()
{
    constexpr int MediaCount = 50000;
    QVector<MediaLibraryItem> items;
    items.reserve(MediaCount);
    for (int index = 0; index < MediaCount; ++index) {
        MediaLibraryItem item;
        item.id = index + 1;
        item.fileName = QStringLiteral("video-%1.mp4").arg(index, 5, 10, QLatin1Char('0'));
        items.append(item);
    }

    MediaLibraryModel model;
    model.setItems(std::move(items));

    QElapsedTimer timer;
    timer.start();
    QCOMPARE(model.indexOfId(MediaCount), MediaCount - 1);
    QVERIFY2(timer.elapsed() < 50, qPrintable(QStringLiteral("indexOfId took %1 ms").arg(timer.elapsed())));

    QSignalSpy favoriteSpy(&model, &MediaLibraryModel::dataChanged);
    QVERIFY(model.setFavorite(MediaCount, true));
    QCOMPARE(favoriteSpy.size(), 1);
    QList<QVariant> args = favoriteSpy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), MediaCount - 1);
    QCOMPARE(args.at(1).toModelIndex().row(), MediaCount - 1);
    QCOMPARE(args.at(2).value<QList<int>>(), QList<int>({MediaLibraryModel::IsFavoriteRole}));

    MediaLibraryItem replacement;
    replacement.id = MediaCount;
    replacement.fileName = QStringLiteral("replacement.mp4");
    QSignalSpy replaceSpy(&model, &MediaLibraryModel::dataChanged);
    QVERIFY(model.replaceItem(MediaCount, replacement));
    QCOMPARE(replaceSpy.size(), 1);
    args = replaceSpy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), MediaCount - 1);
    QCOMPARE(args.at(1).toModelIndex().row(), MediaCount - 1);
}

void M11PerformanceSecurityRegressionTest::localSecurityHardeningRejectsUnsafeInputs()
{
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    ExternalToolService toolService;
    const ExternalToolStatus directoryTool = toolService.validateFfmpeg(tempDir.path());
    QVERIFY(!directoryTool.succeeded);
    QVERIFY(directoryTool.configuredPathUsed);

    const QString textToolPath = tempDir.filePath(QStringLiteral("tool.txt"));
    QVERIFY(writeFile(textToolPath));
    const ExternalToolStatus textTool = toolService.validateFfmpeg(textToolPath);
    QVERIFY(!textTool.succeeded);
    QVERIFY(textTool.errorMessage.contains(QStringLiteral(".exe")));

    const ExternalToolStatus pathTool = toolService.validateFfprobe(QString());
    if (QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty()) {
        QVERIFY(!pathTool.succeeded);
        QVERIFY(pathTool.errorMessage.contains(QStringLiteral("PATH")));
    } else {
        QVERIFY(pathTool.resolvedProgram.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive));
    }

    QString clearError;
    const QString snapshotRoot = SnapshotService::defaultSnapshotRoot();
    QVERIFY(QDir().mkpath(snapshotRoot));
    QVERIFY(writeFile(QDir(snapshotRoot).filePath(QStringLiteral("snapshot.jpg"))));
    QVERIFY2(SnapshotService::clearSnapshotRoot(snapshotRoot, &clearError), qPrintable(clearError));
    QVERIFY(!QDir(snapshotRoot).exists());

    const QString thumbnailRoot = ThumbnailService::defaultThumbnailRoot();
    QVERIFY(QDir().mkpath(thumbnailRoot));
    QVERIFY(writeFile(QDir(thumbnailRoot).filePath(QStringLiteral("thumbnail.jpg"))));
    QVERIFY2(ThumbnailService::clearThumbnailRoot(thumbnailRoot, &clearError), qPrintable(clearError));
    QVERIFY(!QDir(thumbnailRoot).exists());

    const QString outsideRoot = tempDir.filePath(QStringLiteral("outside"));
    QVERIFY(QDir().mkpath(outsideRoot));
    QVERIFY(!SnapshotService::clearSnapshotRoot(outsideRoot, &clearError));
    QVERIFY(clearError.contains(QStringLiteral("outside")));
    QVERIFY(!ThumbnailService::clearThumbnailRoot(outsideRoot, &clearError));
    QVERIFY(clearError.contains(QStringLiteral("outside")));

    TestDatabase testDatabase = createDatabase(tempDir.filePath(QStringLiteral("security.sqlite3")));
    QString databaseError;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &databaseError), qPrintable(databaseError));
    ScannedMediaFile media;
    media.fileName = QStringLiteral("unsafe-thumb.mp4");
    media.filePath = QDir::toNativeSeparators(tempDir.filePath(QStringLiteral("unsafe-thumb.mp4")));
    media.fileExtension = QStringLiteral(".mp4");
    media.fileSize = 1024;
    media.modifiedAt = QStringLiteral("2026-04-20T00:00:00.000Z");
    {
        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.upsertScanResult(tempDir.path(), {media}), qPrintable(repository.lastError()));
        QVERIFY2(repository.setMediaThumbnailPath(1, QDir::toNativeSeparators(tempDir.filePath(QStringLiteral("external.jpg")))), qPrintable(repository.lastError()));
        const QVector<MediaLibraryItem> fetchedItems = repository.fetchLibraryItems();
        QCOMPARE(fetchedItems.size(), 1);
        QCOMPARE(fetchedItems.first().thumbnailPath, QString());
    }
    closeDatabase(&testDatabase);

    const QString powershell = QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (powershell.isEmpty()) {
        QSKIP("powershell is not available for process output cap verification.");
    }

    ProcessRunner runner;
    const ProcessResult outputCapped = runner.run(
        powershell,
        {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-Command"),
            QStringLiteral("[Console]::Out.Write(('x' * 1100000))"),
        },
        10000);
    QVERIFY(!outputCapped.succeeded);
    QVERIFY(outputCapped.errorMessage.contains(QStringLiteral("too much output")));
    QVERIFY(outputCapped.standardOutput.size() <= 1024 * 1024);
}

QTEST_MAIN(M11PerformanceSecurityRegressionTest)

#include "M11PerformanceSecurityRegressionTest.moc"

#include "core/PathSecurity.h"
#include "db/MediaRepository.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"
#include "TestSupport.h"

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {
QStringList *g_sqlConnectionWarnings = nullptr;

void captureSqlConnectionWarnings(QtMsgType, const QMessageLogContext &, const QString &message)
{
    if (g_sqlConnectionWarnings
        && message.contains(QStringLiteral("QSqlDatabasePrivate::removeDatabase"))) {
        g_sqlConnectionWarnings->append(message);
    }
}
}

class M12UtilityModularizationTest : public QObject
{
    Q_OBJECT

private slots:
    void pathSecurityNormalizesAndRedactsLocalPaths();
    void managedRootsRejectOutsideCleanup();
    void testDatabaseFixtureClosesWithoutConnectionWarnings();
};

void M12UtilityModularizationTest::pathSecurityNormalizesAndRedactsLocalPaths()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path();
    const QString child = QDir(root).filePath(QStringLiteral("nested/file.mp4"));
    const QString siblingPrefix = root + QStringLiteral("_suffix/file.mp4");

    QVERIFY(!PathSecurity::pathForComparison(root).isEmpty());
    QVERIFY(PathSecurity::samePath(root, QDir(root).filePath(QStringLiteral("."))));
    QVERIFY(PathSecurity::isInsideOrEqual(root, root));
    QVERIFY(PathSecurity::isInsideOrEqual(child, root));
    QVERIFY(!PathSecurity::isInsideOrEqual(siblingPrefix, root));

    const QString appDataFile = QDir(PathSecurity::appDataPath()).filePath(QStringLiteral("pickle.sqlite3"));
    QCOMPARE(PathSecurity::redactedLocalPath(appDataFile), QStringLiteral("<app-data>/pickle.sqlite3"));

    const QString homeFile = QDir(QDir::homePath()).filePath(QStringLiteral("private-video.mp4"));
    QCOMPARE(PathSecurity::redactedLocalPath(homeFile), QStringLiteral("<home>/private-video.mp4"));

    const QString externalFile = QDir(QStringLiteral("E:/Documents/Temp")).filePath(QStringLiteral("clip.mp4"));
    QCOMPARE(PathSecurity::redactedLocalPath(externalFile), QStringLiteral("<path>/clip.mp4"));
}

void M12UtilityModularizationTest::managedRootsRejectOutsideCleanup()
{
    QStandardPaths::setTestModeEnabled(true);

    QString errorMessage;
    const QString snapshotRoot = SnapshotService::defaultSnapshotRoot();
    QVERIFY(QDir().mkpath(snapshotRoot));
    QVERIFY(PickleTest::writeFile(QDir(snapshotRoot).filePath(QStringLiteral("snapshot.jpg"))));
    QVERIFY2(SnapshotService::clearSnapshotRoot(snapshotRoot, &errorMessage), qPrintable(errorMessage));
    QVERIFY(!QDir(snapshotRoot).exists());

    const QString thumbnailRoot = ThumbnailService::defaultThumbnailRoot();
    QVERIFY(QDir().mkpath(thumbnailRoot));
    QVERIFY(PickleTest::writeFile(QDir(thumbnailRoot).filePath(QStringLiteral("thumbnail.jpg"))));
    QVERIFY2(ThumbnailService::clearThumbnailRoot(thumbnailRoot, &errorMessage), qPrintable(errorMessage));
    QVERIFY(!QDir(thumbnailRoot).exists());

    QTemporaryDir outsideRoot;
    QVERIFY(outsideRoot.isValid());
    QVERIFY(!SnapshotService::clearSnapshotRoot(outsideRoot.path(), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("outside")));
    QVERIFY(!ThumbnailService::clearThumbnailRoot(outsideRoot.path(), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("outside")));
}

void M12UtilityModularizationTest::testDatabaseFixtureClosesWithoutConnectionWarnings()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    PickleTest::TestDatabase database = PickleTest::createDatabase(databaseDir.filePath(QStringLiteral("fixture.sqlite3")));
    QString errorMessage;
    QVERIFY2(PickleTest::initializeMigratedDatabase(&database, &errorMessage), qPrintable(errorMessage));

    {
        MediaRepository repository(database.database);
        const ScannedMediaFile media = PickleTest::scannedMediaFile(databaseDir.filePath(QStringLiteral("fixture.mp4")));
        QVERIFY2(repository.upsertScanResult(databaseDir.path(), {media}), qPrintable(repository.lastError()));
    }

    QStringList warnings;
    g_sqlConnectionWarnings = &warnings;
    qInstallMessageHandler(captureSqlConnectionWarnings);
    PickleTest::closeDatabase(&database);
    qInstallMessageHandler(nullptr);
    g_sqlConnectionWarnings = nullptr;

    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QLatin1Char('\n'))));
}

QTEST_MAIN(M12UtilityModularizationTest)

#include "M12UtilityModularizationTest.moc"

#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/MetadataService.h"

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
    result.connectionName = QStringLiteral("m06_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    result.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), result.connectionName);
    result.database.setDatabaseName(databasePath);
    return result;
}

int insertMedia(QSqlDatabase database, const QString &fileName, QString *errorMessage = nullptr)
{
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
    query.addBindValue(fileName);
    query.addBindValue(QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
    query.addBindValue(QStringLiteral(".mp4"));
    query.addBindValue(1024);
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

int scalarInt(QSqlDatabase database, const QString &sql)
{
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }

    return query.value(0).toInt();
}
}

class M06MetadataDetailsTest : public QObject
{
    Q_OBJECT

private slots:
    void repositoryUpdatesDetailsAndTags();
    void repositoryUpdatesMetadataAndFetchesDisplayValues();
    void metadataParserParsesFfprobeJson();
    void metadataParserHandlesAudioOnlyAndInvalidJson();
};

void M06MetadataDetailsTest::repositoryUpdatesDetailsAndTags()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));

    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        QString insertError;
        const int mediaId = insertMedia(testDatabase.database, QStringLiteral("details.mp4"), &insertError);
        QVERIFY2(mediaId > 0, qPrintable(insertError));

        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.updateMediaDetails(
                     mediaId,
                     QStringLiteral("A useful note"),
                     QStringLiteral("reviewed"),
                     4,
                     {QStringLiteral(" Action "), QStringLiteral("action"), QStringLiteral("Drama"), QString()}),
                 qPrintable(repository.lastError()));

        QVector<MediaLibraryItem> items = repository.fetchLibraryItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().description, QStringLiteral("A useful note"));
        QCOMPARE(items.first().reviewStatus, QStringLiteral("reviewed"));
        QCOMPARE(items.first().rating, 4);
        QCOMPARE(items.first().tags, QStringList({QStringLiteral("Action"), QStringLiteral("Drama")}));
        QCOMPARE(scalarInt(testDatabase.database, QStringLiteral("SELECT COUNT(*) FROM tags")), 2);
        QCOMPARE(scalarInt(testDatabase.database, QStringLiteral("SELECT COUNT(*) FROM media_tags")), 2);

        QVERIFY(!repository.updateMediaDetails(mediaId, {}, QStringLiteral("reviewed"), 6, {}));
        QVERIFY(!repository.updateMediaDetails(mediaId, {}, QStringLiteral("invalid"), 1, {}));

        QVERIFY2(repository.updateMediaDetails(
                     mediaId,
                     QStringLiteral("Changed"),
                     QStringLiteral("needs_followup"),
                     0,
                     {QStringLiteral("Drama")}),
                 qPrintable(repository.lastError()));

        items = repository.fetchLibraryItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.first().description, QStringLiteral("Changed"));
        QCOMPARE(items.first().reviewStatus, QStringLiteral("needs_followup"));
        QCOMPARE(items.first().rating, 0);
        QCOMPARE(items.first().tags, QStringList({QStringLiteral("Drama")}));
        QCOMPARE(scalarInt(testDatabase.database, QStringLiteral("SELECT COUNT(*) FROM media_tags")), 1);

        testDatabase.database.close();
    }

    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M06MetadataDetailsTest::repositoryUpdatesMetadataAndFetchesDisplayValues()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());

    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("test.sqlite3")));

    {
        QVERIFY(testDatabase.database.open());
        MigrationService migrationService(testDatabase.database);
        QVERIFY2(migrationService.migrate(), qPrintable(migrationService.lastError()));

        QString insertError;
        const int mediaId = insertMedia(testDatabase.database, QStringLiteral("metadata.mp4"), &insertError);
        QVERIFY2(mediaId > 0, qPrintable(insertError));

        MediaMetadata metadata;
        metadata.durationMs = 90500;
        metadata.bitrate = 2400000;
        metadata.frameRate = 29.97003;
        metadata.width = 1920;
        metadata.height = 1080;
        metadata.videoCodec = QStringLiteral("h264");
        metadata.audioCodec = QStringLiteral("aac");

        MediaRepository repository(testDatabase.database);
        QVERIFY2(repository.updateMediaMetadata(mediaId, metadata), qPrintable(repository.lastError()));

        const QVector<MediaLibraryItem> items = repository.fetchLibraryItems();
        QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().durationMs, 90500);
        QCOMPARE(items.first().duration, QStringLiteral("01:30"));
        QCOMPARE(items.first().resolution, QStringLiteral("1920 x 1080"));
        QCOMPARE(items.first().codec, QStringLiteral("h264 / aac"));
        QCOMPARE(items.first().bitrateBps, 2400000);
        QCOMPARE(items.first().bitrate, QStringLiteral("2.4 Mbps"));
        QVERIFY(qAbs(items.first().frameRateValue - 29.97003) < 0.0001);
        QCOMPARE(items.first().frameRate, QStringLiteral("29.97 fps"));

        testDatabase.database.close();
    }

    testDatabase.database = {};
    QSqlDatabase::removeDatabase(testDatabase.connectionName);
}

void M06MetadataDetailsTest::metadataParserParsesFfprobeJson()
{
    const QByteArray json = R"json(
        {
            "streams": [
                {
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 1920,
                    "height": 1080,
                    "avg_frame_rate": "30000/1001"
                },
                {
                    "codec_type": "audio",
                    "codec_name": "aac"
                }
            ],
            "format": {
                "duration": "12.345000",
                "bit_rate": "2400000"
            }
        }
    )json";

    const MetadataExtractionResult result = MetadataService::parseFfprobeJson(json);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(result.metadata.durationMs, 12345);
    QCOMPARE(result.metadata.bitrate, 2400000);
    QCOMPARE(result.metadata.width, 1920);
    QCOMPARE(result.metadata.height, 1080);
    QCOMPARE(result.metadata.videoCodec, QStringLiteral("h264"));
    QCOMPARE(result.metadata.audioCodec, QStringLiteral("aac"));
    QVERIFY(qAbs(result.metadata.frameRate - 29.97003) < 0.001);
}

void M06MetadataDetailsTest::metadataParserHandlesAudioOnlyAndInvalidJson()
{
    const QByteArray audioOnlyJson = R"json(
        {
            "streams": [
                {
                    "codec_type": "audio",
                    "codec_name": "opus",
                    "duration": "45.000000",
                    "bit_rate": "128000"
                }
            ]
        }
    )json";

    MetadataExtractionResult result = MetadataService::parseFfprobeJson(audioOnlyJson);
    QVERIFY2(result.succeeded, qPrintable(result.errorMessage));
    QCOMPARE(result.metadata.durationMs, 45000);
    QCOMPARE(result.metadata.bitrate, 128000);
    QCOMPARE(result.metadata.audioCodec, QStringLiteral("opus"));
    QVERIFY(result.metadata.videoCodec.isEmpty());

    result = MetadataService::parseFfprobeJson(QByteArray("{"), QByteArray("ffprobe stderr"));
    QVERIFY(!result.succeeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("ffprobe stderr")));
}

QTEST_MAIN(M06MetadataDetailsTest)

#include "M06MetadataDetailsTest.moc"

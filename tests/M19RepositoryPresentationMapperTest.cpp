#include "TestSupport.h"
#include "app/MediaActionsController.h"
#include "app/MediaItemPresenter.h"
#include "core/PathSecurity.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"

#include <QDir>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {
using PickleTest::closeDatabase;
using PickleTest::createDatabase;
using PickleTest::initializeMigratedDatabase;
using PickleTest::insertMediaFile;
using PickleTest::TestDatabase;
using PickleTest::writeFile;

bool execPrepared(QSqlDatabase database, const QString &sql, const QVariantList &values, QString *errorMessage)
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }
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
}

class M19RepositoryPresentationMapperTest : public QObject
{
    Q_OBJECT

private slots:
    void repositoryReturnsRawMediaAndPresenterOwnsFormattingAndThumbnailFiltering();
    void modelSingleRowRefreshUsesPresentedItem();
    void metadataRefreshUpdatesModelBeforeAsyncReload();
};

void M19RepositoryPresentationMapperTest::repositoryReturnsRawMediaAndPresenterOwnsFormattingAndThumbnailFiltering()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(writeFile(tempDir.filePath(QStringLiteral("clip.mp4"))));

    TestDatabase testDatabase = createDatabase(tempDir.filePath(QStringLiteral("library.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    const int mediaId = insertMediaFile(testDatabase.database, tempDir.filePath(QStringLiteral("clip.mp4")), &error);
    QVERIFY2(mediaId > 0, qPrintable(error));

    const QString unsafeThumbnail = QDir::toNativeSeparators(tempDir.filePath(QStringLiteral("unsafe.png")));
    QVERIFY2(execPrepared(
                 testDatabase.database,
                 QStringLiteral(R"sql(
                    UPDATE media_files
                    SET duration_ms = ?, width = ?, height = ?, video_codec = ?, audio_codec = ?,
                        bitrate = ?, frame_rate = ?, review_status = ?, rating = ?, thumbnail_path = ?,
                        is_favorite = 1, is_delete_candidate = 1, last_position_ms = ?, last_played_at = ?
                    WHERE id = ?
                 )sql"),
                 {65000, 1920, 1080, QStringLiteral("h264"), QStringLiteral("aac"),
                  2500000, 29.97, QStringLiteral("reviewed"), 4, unsafeThumbnail,
                  1234, QStringLiteral("2026-04-20T01:02:03.000Z"), mediaId},
                 &error),
             qPrintable(error));
    QVERIFY2(execPrepared(testDatabase.database, QStringLiteral("INSERT INTO tags (name) VALUES (?)"), {QStringLiteral("demo")}, &error), qPrintable(error));
    const int tagId = QSqlQuery(testDatabase.database).lastInsertId().toInt();
    Q_UNUSED(tagId);
    QSqlQuery tagQuery(testDatabase.database);
    QVERIFY2(tagQuery.exec(QStringLiteral("SELECT id FROM tags WHERE name = 'demo'")) && tagQuery.next(), qPrintable(tagQuery.lastError().text()));
    QVERIFY2(execPrepared(testDatabase.database, QStringLiteral("INSERT INTO media_tags (media_id, tag_id) VALUES (?, ?)"), {mediaId, tagQuery.value(0)}, &error), qPrintable(error));

    MediaRepository repository(testDatabase.database);
    const QVector<MediaFile> rawFiles = repository.fetchMediaFiles();
    QVERIFY2(repository.lastError().isEmpty(), qPrintable(repository.lastError()));
    QCOMPARE(rawFiles.size(), 1);
    const MediaFile raw = rawFiles.first();
    QCOMPARE(raw.id, mediaId);
    QCOMPARE(raw.thumbnailPath, unsafeThumbnail);
    QCOMPARE(raw.tags, QStringList({QStringLiteral("demo")}));

    const MediaLibraryItem item = MediaItemPresenter::present(raw);
    QCOMPARE(item.fileSize, QStringLiteral("1.0 KB"));
    QCOMPARE(item.duration, QStringLiteral("01:05"));
    QCOMPARE(item.resolution, QStringLiteral("1920 x 1080"));
    QCOMPARE(item.codec, QStringLiteral("h264 / aac"));
    QCOMPARE(item.bitrate, QStringLiteral("2.5 Mbps"));
    QCOMPARE(item.frameRate, QStringLiteral("29.97 fps"));
    QCOMPARE(item.rating, 4);
    QVERIFY(item.isFavorite);
    QVERIFY(item.isDeleteCandidate);
    QCOMPARE(item.lastPositionMs, 1234);
    QVERIFY(item.thumbnailPath.isEmpty());

    MediaFile managedRaw = raw;
    managedRaw.thumbnailPath = QDir::toNativeSeparators(
        QDir(PathSecurity::appDataPath()).filePath(QStringLiteral("thumbnails/thumb.png")));
    QCOMPARE(MediaItemPresenter::present(managedRaw).thumbnailPath, managedRaw.thumbnailPath);

    const MediaFile rawById = repository.fetchMediaFileById(mediaId);
    QCOMPARE(rawById.id, mediaId);
    QCOMPARE(rawById.tags, QStringList({QStringLiteral("demo")}));

    closeDatabase(&testDatabase);
}

void M19RepositoryPresentationMapperTest::modelSingleRowRefreshUsesPresentedItem()
{
    MediaFile raw;
    raw.id = 42;
    raw.fileName = QStringLiteral("clip.mp4");
    raw.filePath = QStringLiteral("C:/Media/clip.mp4");
    raw.fileSizeBytes = 2048;
    raw.durationMs = 3601000;
    raw.width = 1280;
    raw.height = 720;
    raw.videoCodec = QStringLiteral("hevc");
    raw.audioCodec = QStringLiteral("opus");
    raw.rating = 2;

    MediaLibraryModel model;
    model.setItems({MediaItemPresenter::present(raw)});
    QSignalSpy dataChangedSpy(&model, &MediaLibraryModel::dataChanged);

    raw.rating = 5;
    raw.durationMs = 61000;
    QVERIFY(model.replaceItem(raw.id, MediaItemPresenter::present(raw)));
    QCOMPARE(dataChangedSpy.size(), 1);
    QCOMPARE(dataChangedSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataChangedSpy.first().at(1).toModelIndex().row(), 0);
    QCOMPARE(model.itemAt(0).rating, 5);
    QCOMPARE(model.itemAt(0).duration, QStringLiteral("01:01"));
}

void M19RepositoryPresentationMapperTest::metadataRefreshUpdatesModelBeforeAsyncReload()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(writeFile(tempDir.filePath(QStringLiteral("clip.mp4"))));

    TestDatabase testDatabase = createDatabase(tempDir.filePath(QStringLiteral("metadata.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    const int mediaId = insertMediaFile(testDatabase.database, tempDir.filePath(QStringLiteral("clip.mp4")), &error);
    QVERIFY2(mediaId > 0, qPrintable(error));

    MediaRepository repository(testDatabase.database);
    MediaLibraryModel model;
    model.setItems(repository.fetchLibraryItems());
    QCOMPARE(model.itemAt(0).durationMs, 0);
    QCOMPARE(model.itemAt(0).resolution, QStringLiteral("-"));

    MediaMetadata metadata;
    metadata.durationMs = 90500;
    metadata.bitrate = 2400000;
    metadata.frameRate = 29.97003;
    metadata.width = 1920;
    metadata.height = 1080;
    metadata.videoCodec = QStringLiteral("h264");
    metadata.audioCodec = QStringLiteral("aac");
    QVERIFY2(repository.updateMediaMetadata(mediaId, metadata), qPrintable(repository.lastError()));

    MediaActionsController actions(&repository, &model);
    const MediaActionResult refresh = actions.refreshMediaFromRepository(mediaId);
    QVERIFY(refresh.selectedMediaChanged);
    QCOMPARE(model.itemAt(0).durationMs, 90500);
    QCOMPARE(model.itemAt(0).duration, QStringLiteral("01:30"));
    QCOMPARE(model.itemAt(0).resolution, QStringLiteral("1920 x 1080"));
    QCOMPARE(model.itemAt(0).codec, QStringLiteral("h264 / aac"));

    closeDatabase(&testDatabase);
}

QTEST_MAIN(M19RepositoryPresentationMapperTest)

#include "M19RepositoryPresentationMapperTest.moc"

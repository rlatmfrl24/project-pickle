#include "application/RenameMediaUseCase.h"
#include "application/SavePlaybackPositionUseCase.h"
#include "application/UpdateMediaDetailsUseCase.h"
#include "application/UpdateMediaFlagsUseCase.h"
#include "ports/IFileRenamer.h"
#include "ports/IMediaRepository.h"

#include <QtTest/QtTest>

namespace {
class FakeRepository : public IMediaRepository
{
public:
    QString error;
    QString description;
    QString reviewStatus;
    QStringList tags;
    int rating = -1;
    bool favorite = false;
    bool deleteCandidate = false;
    qint64 playbackPosition = -1;
    ScannedMediaFile renamedFile;

    bool upsertScanResult(const QString &, const QVector<ScannedMediaFile> &, MediaUpsertResult *) override { return true; }
    QVector<MediaLibraryItem> fetchLibraryItems() override { return {}; }
    QVector<MediaLibraryItem> fetchLibraryItems(const MediaLibraryQuery &) override { return {}; }
    MediaLibraryItem fetchMediaById(int) override { return {}; }
    bool renameMediaFile(int, const ScannedMediaFile &file) override
    {
        renamedFile = file;
        return error.isEmpty();
    }
    bool setMediaFavorite(int, bool enabled) override
    {
        favorite = enabled;
        return error.isEmpty();
    }
    bool setMediaDeleteCandidate(int, bool enabled) override
    {
        deleteCandidate = enabled;
        return error.isEmpty();
    }
    bool updatePlaybackPosition(int, qint64 positionMs, const QString &) override
    {
        playbackPosition = positionMs;
        return error.isEmpty();
    }
    bool resetLibraryData() override { return true; }
    bool updateMediaMetadata(int, const MediaMetadata &) override { return true; }
    bool updateMediaDetails(int, const QString &nextDescription, const QString &nextReviewStatus, int nextRating, const QStringList &nextTags) override
    {
        description = nextDescription;
        reviewStatus = nextReviewStatus;
        rating = nextRating;
        tags = nextTags;
        return error.isEmpty();
    }
    bool addSnapshot(int, const QString &, qint64, int *) override { return true; }
    QVector<SnapshotItem> fetchSnapshotsForMedia(int) override { return {}; }
    bool setMediaThumbnailPath(int, const QString &) override { return true; }
    QVector<ThumbnailBackfillItem> fetchThumbnailBackfillItems() override { return {}; }
    QString lastError() const override { return error; }
};

class FakeRenamer : public IFileRenamer
{
public:
    FileRenameResult nextResult;
    QString rollbackBaseName;

    FileRenameResult renameFile(const QString &, const QString &newBaseName) const override
    {
        if (newBaseName == QStringLiteral("original")) {
            const_cast<FakeRenamer *>(this)->rollbackBaseName = newBaseName;
        }
        return nextResult;
    }
};
}

class M15UseCaseMediaActionsTest : public QObject
{
    Q_OBJECT

private slots:
    void updatesDetailsFlagsAndPlaybackThroughRepositoryPort();
    void renameRollsBackWhenRepositoryUpdateFails();
};

void M15UseCaseMediaActionsTest::updatesDetailsFlagsAndPlaybackThroughRepositoryPort()
{
    FakeRepository repository;

    UpdateMediaDetailsUseCase details(&repository);
    QVERIFY(details.execute(1, QStringLiteral("note"), QStringLiteral("reviewed"), 4, {QStringLiteral("tag")}).succeeded);
    QCOMPARE(repository.description, QStringLiteral("note"));
    QCOMPARE(repository.reviewStatus, QStringLiteral("reviewed"));
    QCOMPARE(repository.rating, 4);
    QCOMPARE(repository.tags, QStringList({QStringLiteral("tag")}));

    UpdateMediaFlagsUseCase flags(&repository);
    QVERIFY(flags.execute(1, MediaFlagKind::Favorite, true).succeeded);
    QVERIFY(repository.favorite);
    QVERIFY(flags.execute(1, MediaFlagKind::DeleteCandidate, true).succeeded);
    QVERIFY(repository.deleteCandidate);

    SavePlaybackPositionUseCase playback(&repository);
    QVERIFY(playback.execute(1, 1234, QStringLiteral("now")).succeeded);
    QCOMPARE(repository.playbackPosition, 1234);
}

void M15UseCaseMediaActionsTest::renameRollsBackWhenRepositoryUpdateFails()
{
    FakeRepository repository;
    repository.error = QStringLiteral("db failed");

    FakeRenamer renamer;
    renamer.nextResult.succeeded = true;
    renamer.nextResult.file.fileName = QStringLiteral("renamed.mp4");
    renamer.nextResult.file.filePath = QStringLiteral("C:/Media/renamed.mp4");

    RenameMediaUseCase rename(&renamer, &repository);
    const OperationResult<ScannedMediaFile> result = rename.execute(
        1,
        QStringLiteral("C:/Media/original.mp4"),
        QStringLiteral("renamed"),
        QStringLiteral("original"));

    QVERIFY(!result.succeeded);
    QCOMPARE(result.errorMessage, QStringLiteral("db failed"));
    QCOMPARE(renamer.rollbackBaseName, QStringLiteral("original"));
}

QTEST_MAIN(M15UseCaseMediaActionsTest)

#include "M15UseCaseMediaActionsTest.moc"

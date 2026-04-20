#include "application/LoadLibraryUseCase.h"
#include "application/ScanLibraryUseCase.h"
#include "ports/IFileScanner.h"
#include "ports/IMediaRepository.h"

#include <QtTest/QtTest>

namespace {
class FakeRepository : public IMediaRepository
{
public:
    QVector<MediaLibraryItem> libraryItems;
    QVector<ScannedMediaFile> committedFiles;
    QString committedRoot;
    QString error;

    bool upsertScanResult(const QString &rootPath, const QVector<ScannedMediaFile> &files, MediaUpsertResult *result = nullptr) override
    {
        committedRoot = rootPath;
        committedFiles = files;
        if (result) {
            result->scannedFileCount = files.size();
            result->upsertedFileCount = files.size();
        }
        return error.isEmpty();
    }

    QVector<MediaLibraryItem> fetchLibraryItems() override { return libraryItems; }
    QVector<MediaLibraryItem> fetchLibraryItems(const MediaLibraryQuery &query) override
    {
        lastQuery = query;
        return libraryItems;
    }
    MediaLibraryItem fetchMediaById(int mediaId) override
    {
        for (const MediaLibraryItem &item : libraryItems) {
            if (item.id == mediaId) {
                return item;
            }
        }
        return {};
    }
    bool renameMediaFile(int, const ScannedMediaFile &) override { return true; }
    bool setMediaFavorite(int, bool) override { return true; }
    bool setMediaDeleteCandidate(int, bool) override { return true; }
    bool updatePlaybackPosition(int, qint64, const QString &) override { return true; }
    bool resetLibraryData() override { return true; }
    bool updateMediaMetadata(int, const MediaMetadata &) override { return true; }
    bool updateMediaDetails(int, const QString &, const QString &, int, const QStringList &) override { return true; }
    bool addSnapshot(int, const QString &, qint64, int *) override { return true; }
    QVector<SnapshotItem> fetchSnapshotsForMedia(int) override { return {}; }
    bool setMediaThumbnailPath(int, const QString &) override { return true; }
    QVector<ThumbnailBackfillItem> fetchThumbnailBackfillItems() override { return {}; }
    QString lastError() const override { return error; }

    MediaLibraryQuery lastQuery;
};

class FakeScanner : public IFileScanner
{
public:
    DirectoryScanResult nextResult;
    int progressCalls = 0;

    DirectoryScanResult scanDirectory(
        const QString &rootPath,
        const std::shared_ptr<CancellationToken> &,
        const ProgressCallback &progressCallback = {}) const override
    {
        DirectoryScanResult result = nextResult;
        result.rootPath = rootPath;
        if (progressCallback) {
            ScanProgress progress;
            progress.rootPath = rootPath;
            progress.visitedFileCount = 1;
            progress.matchedFileCount = result.files.size();
            progressCallback(progress);
            ++const_cast<FakeScanner *>(this)->progressCalls;
        }
        return result;
    }
};
}

class M14UseCaseLibraryScanTest : public QObject
{
    Q_OBJECT

private slots:
    void loadLibraryUsesRepositoryAndPreservesGeneration();
    void scanLibraryCommitsScanResultAndClearsPayload();
};

void M14UseCaseLibraryScanTest::loadLibraryUsesRepositoryAndPreservesGeneration()
{
    FakeRepository repository;
    MediaLibraryItem item;
    item.id = 7;
    item.fileName = QStringLiteral("clip.mp4");
    repository.libraryItems = {item};

    MediaLibraryQuery query;
    query.searchText = QStringLiteral("clip");
    query.sortKey = MediaLibrarySortKey::Modified;
    LoadLibraryUseCase useCase(&repository);
    const LibraryLoadResult result = useCase.execute(query, 42);

    QVERIFY(result.succeeded);
    QCOMPARE(result.generation, 42);
    QCOMPARE(result.query.searchText, QStringLiteral("clip"));
    QCOMPARE(repository.lastQuery.sortKey, MediaLibrarySortKey::Modified);
    QCOMPARE(result.items.size(), 1);
    QCOMPARE(result.items.first().id, 7);
}

void M14UseCaseLibraryScanTest::scanLibraryCommitsScanResultAndClearsPayload()
{
    FakeScanner scanner;
    ScannedMediaFile file;
    file.fileName = QStringLiteral("scan.mp4");
    scanner.nextResult.rootPath = QStringLiteral("C:/Media");
    scanner.nextResult.files = {file};
    scanner.nextResult.visitedFileCount = 3;
    scanner.nextResult.matchedFileCount = 1;
    scanner.nextResult.succeeded = true;

    FakeRepository repository;
    ScanLibraryUseCase useCase(&scanner, &repository);
    int forwardedProgressCount = 0;
    const ScanCommitResult result = useCase.execute(
        QStringLiteral("C:/Media"),
        nullptr,
        [&forwardedProgressCount](const ScanProgress &progress) {
            ++forwardedProgressCount;
            QCOMPARE(progress.rootPath, QStringLiteral("C:/Media"));
        });

    QVERIFY(result.succeeded);
    QCOMPARE(scanner.progressCalls, 1);
    QCOMPARE(forwardedProgressCount, 1);
    QCOMPARE(repository.committedRoot, QStringLiteral("C:/Media"));
    QCOMPARE(repository.committedFiles.size(), 1);
    QCOMPARE(result.upsert.upsertedFileCount, 1);
    QVERIFY(result.scan.files.isEmpty());
}

QTEST_MAIN(M14UseCaseLibraryScanTest)

#include "M14UseCaseLibraryScanTest.moc"

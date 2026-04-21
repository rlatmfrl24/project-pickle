#include "app/AppController.h"
#include "application/BatchRenameMediaUseCase.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "media/RenameService.h"
#include "ports/IFileRenamer.h"
#include "ports/IMediaRepository.h"
#include "TestSupport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest/QtTest>

namespace {
using PickleTest::closeDatabase;
using PickleTest::createDatabase;
using PickleTest::initializeMigratedDatabase;
using PickleTest::insertMediaFile;
using PickleTest::TestDatabase;
using PickleTest::writeFile;

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

MediaLibraryItem mediaItem(int id, const QString &filePath)
{
    const QFileInfo info(filePath);
    MediaLibraryItem item;
    item.id = id;
    item.fileName = info.fileName();
    item.filePath = QDir::toNativeSeparators(info.absoluteFilePath());
    return item;
}

BatchRenameRule renameRule(
    const QString &prefix,
    const QString &suffix = QString(),
    bool numberingEnabled = false,
    int numberStart = 1,
    int numberPadding = 2)
{
    BatchRenameRule rule;
    rule.prefix = prefix;
    rule.suffix = suffix;
    rule.numberingEnabled = numberingEnabled;
    rule.numberStart = numberStart;
    rule.numberPadding = numberPadding;
    return rule;
}

class ConstantTargetRenamer final : public IFileRenamer
{
public:
    FileRenameResult planRename(const QString &, const QString &) const override
    {
        FileRenameResult result;
        result.file.fileName = QStringLiteral("shared.mp4");
        result.file.filePath = QDir::toNativeSeparators(QStringLiteral("C:/Media/shared.mp4"));
        result.file.fileExtension = QStringLiteral(".mp4");
        result.succeeded = true;
        return result;
    }

    FileRenameResult renameFile(const QString &, const QString &) const override
    {
        return planRename({}, {});
    }
};

class RecordingRenamer final : public IFileRenamer
{
public:
    RecordingRenamer(QStringList *calls, QStringList *rollbackBaseNames)
        : m_calls(calls)
        , m_rollbackBaseNames(rollbackBaseNames)
    {
    }

    FileRenameResult planRename(const QString &filePath, const QString &newBaseName) const override
    {
        return makeResult(filePath, newBaseName);
    }

    FileRenameResult renameFile(const QString &filePath, const QString &newBaseName) const override
    {
        if (m_calls) {
            m_calls->append(QStringLiteral("%1 -> %2").arg(filePath, newBaseName));
        }
        if (m_rollbackBaseNames && !newBaseName.startsWith(QStringLiteral("renamed-"))) {
            m_rollbackBaseNames->append(newBaseName);
        }
        return makeResult(filePath, newBaseName);
    }

private:
    QStringList *m_calls = nullptr;
    QStringList *m_rollbackBaseNames = nullptr;

    static FileRenameResult makeResult(const QString &filePath, const QString &newBaseName)
    {
        const QFileInfo sourceInfo(filePath);
        const QString targetFileName = QStringLiteral("%1.%2").arg(newBaseName, sourceInfo.suffix());
        const QString targetPath = sourceInfo.dir().absoluteFilePath(targetFileName);

        FileRenameResult result;
        result.file.fileName = targetFileName;
        result.file.filePath = QDir::toNativeSeparators(targetPath);
        result.file.fileExtension = QStringLiteral(".%1").arg(sourceInfo.suffix().toLower());
        result.file.fileSize = 1024;
        result.file.modifiedAt = QStringLiteral("2026-04-20T00:00:00.000Z");
        result.succeeded = true;
        return result;
    }
};

class FailingRepository final : public IMediaRepository
{
public:
    QVector<MediaLibraryItem> rows;
    int failRenameId = -1;
    QVector<int> renamedIds;

    bool upsertScanResult(const QString &, const QVector<ScannedMediaFile> &, MediaUpsertResult *) override
    {
        return false;
    }

    QVector<MediaLibraryItem> fetchLibraryItems() override
    {
        return rows;
    }

    QVector<MediaLibraryItem> fetchLibraryItems(const MediaLibraryQuery &) override
    {
        return rows;
    }

    MediaLibraryItem fetchMediaById(int mediaId) override
    {
        for (const MediaLibraryItem &row : rows) {
            if (row.id == mediaId) {
                return row;
            }
        }
        return {};
    }

    bool renameMediaFile(int mediaId, const ScannedMediaFile &file) override
    {
        if (mediaId == failRenameId) {
            m_lastError = QStringLiteral("db update failed");
            return false;
        }
        for (MediaLibraryItem &row : rows) {
            if (row.id == mediaId) {
                row.fileName = file.fileName;
                row.filePath = file.filePath;
                renamedIds.append(mediaId);
                m_lastError.clear();
                return true;
            }
        }
        m_lastError = QStringLiteral("media row was not found");
        return false;
    }

    bool setMediaFavorite(int, bool) override { return false; }
    bool setMediaDeleteCandidate(int, bool) override { return false; }
    bool updatePlaybackPosition(int, qint64, const QString &) override { return false; }
    bool resetLibraryData() override { return false; }
    bool updateMediaMetadata(int, const MediaMetadata &) override { return false; }
    bool updateMediaDetails(int, const QString &, const QString &, int, const QStringList &) override { return false; }
    bool addSnapshot(int, const QString &, qint64, int *) override { return false; }
    QVector<SnapshotItem> fetchSnapshotsForMedia(int) override { return {}; }
    bool setMediaThumbnailPath(int, const QString &) override { return false; }
    QVector<ThumbnailBackfillItem> fetchThumbnailBackfillItems() override { return {}; }
    QString lastError() const override { return m_lastError; }

private:
    QString m_lastError;
};
}

class M24BatchRenameTest : public QObject
{
    Q_OBJECT

private slots:
    void previewBuildsNamesInVisibleOrderAndValidatesTargets();
    void previewDetectsInternalTargetCollisions();
    void executionRenamesFilesAndUpdatesRepository();
    void executionContinuesAfterPartialFailureAndRollsBackDbFailures();
    void qmlFacingBatchRenameUiIsWired();
};

void M24BatchRenameTest::previewBuildsNamesInVisibleOrderAndValidatesTargets()
{
    QTemporaryDir mediaDir;
    QVERIFY(mediaDir.isValid());

    const QString alphaPath = mediaDir.filePath(QStringLiteral("alpha.mp4"));
    const QString bravoPath = mediaDir.filePath(QStringLiteral("bravo.mp4"));
    const QString conflictPath = mediaDir.filePath(QStringLiteral("taken-alpha.mp4"));
    QVERIFY(writeFile(alphaPath));
    QVERIFY(writeFile(bravoPath));
    QVERIFY(writeFile(conflictPath));

    const QVector<MediaLibraryItem> items = {
        mediaItem(1, alphaPath),
        mediaItem(2, bravoPath),
    };
    RenameService renamer;
    BatchRenameMediaUseCase useCase(&renamer, nullptr);

    const BatchRenamePreviewResult numbered = useCase.preview(
        items,
        renameRule(QStringLiteral("pre-"), QStringLiteral("-done"), true, 3, 2));
    QCOMPARE(numbered.items.size(), 2);
    QCOMPARE(numbered.runnableCount, 2);
    QCOMPARE(numbered.items.at(0).newName, QStringLiteral("pre-alpha-done-03.mp4"));
    QCOMPARE(numbered.items.at(1).newName, QStringLiteral("pre-bravo-done-04.mp4"));
    QVERIFY(!QFileInfo::exists(numbered.items.at(0).newPath));

    const BatchRenamePreviewResult invalidCharacter = useCase.preview(items, renameRule(QStringLiteral("bad/")));
    QCOMPARE(invalidCharacter.runnableCount, 0);
    QVERIFY(invalidCharacter.items.at(0).status.contains(QStringLiteral("invalid character"), Qt::CaseInsensitive));

    const BatchRenamePreviewResult trailingDot = useCase.preview(items, renameRule({}, QStringLiteral(".")));
    QCOMPARE(trailingDot.runnableCount, 0);
    QVERIFY(trailingDot.items.at(0).status.contains(QStringLiteral("dot or space"), Qt::CaseInsensitive));

    const BatchRenamePreviewResult conflict = useCase.preview(items, renameRule(QStringLiteral("taken-")));
    QCOMPARE(conflict.runnableCount, 1);
    QCOMPARE(conflict.items.at(0).runnable, false);
    QVERIFY(conflict.items.at(0).status.contains(QStringLiteral("already exists"), Qt::CaseInsensitive));

    const BatchRenamePreviewResult unchanged = useCase.preview(items, renameRule(QString()));
    QCOMPARE(unchanged.runnableCount, 0);
    QVERIFY(unchanged.items.at(0).status.contains(QStringLiteral("Unchanged")));

    const QVector<MediaLibraryItem> withMissing = {
        mediaItem(1, alphaPath),
        mediaItem(3, mediaDir.filePath(QStringLiteral("missing.mp4"))),
    };
    const BatchRenamePreviewResult missing = useCase.preview(withMissing, renameRule(QStringLiteral("new-")));
    QCOMPARE(missing.items.at(1).runnable, false);
    QVERIFY(missing.items.at(1).status.contains(QStringLiteral("not found"), Qt::CaseInsensitive));
}

void M24BatchRenameTest::previewDetectsInternalTargetCollisions()
{
    ConstantTargetRenamer renamer;
    BatchRenameMediaUseCase useCase(&renamer, nullptr);
    const QVector<MediaLibraryItem> items = {
        mediaItem(1, QStringLiteral("C:/Media/alpha.mp4")),
        mediaItem(2, QStringLiteral("C:/Media/bravo.mp4")),
    };

    const BatchRenamePreviewResult result = useCase.preview(items, renameRule(QStringLiteral("ignored-")));
    QCOMPARE(result.items.size(), 2);
    QCOMPARE(result.runnableCount, 0);
    QCOMPARE(result.invalidCount, 2);
    QVERIFY(result.items.at(0).status.contains(QStringLiteral("duplicates"), Qt::CaseInsensitive));
    QVERIFY(result.items.at(1).status.contains(QStringLiteral("duplicates"), Qt::CaseInsensitive));
}

void M24BatchRenameTest::executionRenamesFilesAndUpdatesRepository()
{
    QTemporaryDir workingDir;
    QVERIFY(workingDir.isValid());
    TestDatabase testDatabase = createDatabase(workingDir.filePath(QStringLiteral("m24-controller.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));

    const QString alphaPath = workingDir.filePath(QStringLiteral("alpha.mp4"));
    const QString bravoPath = workingDir.filePath(QStringLiteral("bravo.mp4"));
    QVERIFY(writeFile(alphaPath));
    QVERIFY(writeFile(bravoPath));
    const int alphaId = insertMediaFile(testDatabase.database, alphaPath, &error);
    QVERIFY2(alphaId > 0, qPrintable(error));
    const int bravoId = insertMediaFile(testDatabase.database, bravoPath, &error);
    QVERIFY2(bravoId > 0, qPrintable(error));
    QSqlQuery metadataQuery(testDatabase.database);
    QVERIFY2(metadataQuery.exec(QStringLiteral(R"sql(
        UPDATE media_files
        SET duration_ms = 1000,
            width = 1920,
            height = 1080,
            video_codec = 'h264',
            audio_codec = 'aac'
    )sql")), qPrintable(metadataQuery.lastError().text()));

    {
        MediaRepository repository(testDatabase.database);
        RenameService renamer;
        BatchRenameMediaUseCase useCase(&renamer, &repository);
        const QVector<MediaLibraryItem> items = {
            repository.fetchMediaById(alphaId),
            repository.fetchMediaById(bravoId),
        };

        const BatchRenameRule rule = renameRule(QStringLiteral("done-"), QString(), true, 1, 2);
        const BatchRenamePreviewResult preview = useCase.preview(items, rule);
        QCOMPARE(preview.runnableCount, 2);
        QCOMPARE(preview.items.size(), 2);

        const BatchRenameExecutionResult execution = useCase.execute(items, rule);
        QCOMPARE(execution.successCount, 2);
        QCOMPARE(execution.failureCount, 0);
        QCOMPARE(execution.items.size(), 2);
        for (const BatchRenamePreviewItem &item : execution.items) {
            QVERIFY(item.succeeded);
            const QString newPath = item.newPath;
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(newPath), 1000);
            QCOMPARE(
                repository.fetchMediaFileById(item.mediaId).fileName,
                item.newName);
        }
    }

    closeDatabase(&testDatabase);
}

void M24BatchRenameTest::executionContinuesAfterPartialFailureAndRollsBackDbFailures()
{
    QStringList renamerCalls;
    QStringList rollbackBaseNames;
    RecordingRenamer renamer(&renamerCalls, &rollbackBaseNames);
    FailingRepository repository;
    repository.failRenameId = 2;
    repository.rows = {
        mediaItem(1, QStringLiteral("C:/Media/alpha.mp4")),
        mediaItem(2, QStringLiteral("C:/Media/bravo.mp4")),
        mediaItem(3, QStringLiteral("C:/Media/charlie.mp4")),
    };

    BatchRenameMediaUseCase useCase(&renamer, &repository);
    const BatchRenameExecutionResult result = useCase.execute(repository.rows, renameRule(QStringLiteral("renamed-")));

    QCOMPARE(result.successCount, 2);
    QCOMPARE(result.failureCount, 1);
    QCOMPARE(result.skippedCount, 0);
    QCOMPARE(repository.renamedIds, QVector<int>({1, 3}));
    QCOMPARE(renamerCalls.size(), 4);
    QCOMPARE(rollbackBaseNames, QStringList({QStringLiteral("bravo")}));
    QVERIFY(result.items.at(1).status.contains(QStringLiteral("db update failed"), Qt::CaseInsensitive));
}

void M24BatchRenameTest::qmlFacingBatchRenameUiIsWired()
{
    MediaLibraryModel model;
    FailingRepository repository;
    AppController controller(&model, &repository);
    const QMetaObject *metaObject = controller.metaObject();
    QVERIFY(metaObject->indexOfMethod("previewSelectedMediaBatchRename(QVariantMap)") >= 0);
    QVERIFY(metaObject->indexOfMethod("renameSelectedMediaBatch(QVariantMap)") >= 0);

    const QString mainQml = readTextFile(sourcePath(QStringLiteral("qml/Main.qml")));
    const QString infoPanelQml = readTextFile(sourcePath(QStringLiteral("qml/components/InfoPanel.qml")));
    QVERIFY2(!mainQml.isEmpty(), "Could not read Main.qml");
    QVERIFY2(!infoPanelQml.isEmpty(), "Could not read InfoPanel.qml");

    QVERIFY(mainQml.contains(QStringLiteral("batchRenameDialog")));
    QVERIFY(mainQml.contains(QStringLiteral("previewSelectedMediaBatchRename")));
    QVERIFY(mainQml.contains(QStringLiteral("renameSelectedMediaBatch")));
    QVERIFY(mainQml.contains(QStringLiteral("Current name")));
    QVERIFY(mainQml.contains(QStringLiteral("New name")));
    QVERIFY(mainQml.contains(QStringLiteral("Status")));
    QVERIFY(mainQml.contains(QStringLiteral("releaseLoadedSource()")));
    QVERIFY(infoPanelQml.contains(QStringLiteral("signal batchRenameRequested")));
    QVERIFY(infoPanelQml.contains(QStringLiteral("Rename...")));
}

QTEST_MAIN(M24BatchRenameTest)

#include "M24BatchRenameTest.moc"

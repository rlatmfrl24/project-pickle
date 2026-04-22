#include "app/AppController.h"
#include "db/MediaRepository.h"
#include "media/MediaLibraryModel.h"
#include "TestSupport.h"

#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {
using PickleTest::closeDatabase;
using PickleTest::createDatabase;
using PickleTest::initializeMigratedDatabase;
using PickleTest::TestDatabase;

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

int insertMedia(QSqlDatabase database, const QString &fileName, QString *errorMessage = nullptr)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (file_name, file_path, file_extension, file_size, modified_at)
        VALUES (?, ?, '.mp4', 1000, '2026-04-20T00:00:00.000Z')
    )sql"));
    query.addBindValue(fileName);
    query.addBindValue(QDir::toNativeSeparators(QStringLiteral("C:/Media/%1").arg(fileName)));
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return 0;
    }
    if (errorMessage) errorMessage->clear();
    return query.lastInsertId().toInt();
}

QVector<int> seedRows(QSqlDatabase database, QString *errorMessage = nullptr)
{
    QVector<int> ids;
    for (const QString &name : {QStringLiteral("alpha.mp4"), QStringLiteral("bravo.mp4"), QStringLiteral("charlie.mp4")}) {
        ids.append(insertMedia(database, name, errorMessage));
        if (errorMessage && !errorMessage->isEmpty()) {
            return ids;
        }
    }
    return ids;
}
}

class M27KeyboardReviewModeTest : public QObject
{
    Q_OBJECT

private slots:
    void controllerSelectRelativeMovesSingleSelectionSafely();
    void qmlReviewShortcutLayerIsWired();
};

void M27KeyboardReviewModeTest::controllerSelectRelativeMovesSingleSelectionSafely()
{
    QTemporaryDir databaseDir;
    QVERIFY(databaseDir.isValid());
    TestDatabase testDatabase = createDatabase(databaseDir.filePath(QStringLiteral("m27-controller.sqlite3")));
    QString error;
    QVERIFY2(initializeMigratedDatabase(&testDatabase, &error), qPrintable(error));
    const QVector<int> ids = seedRows(testDatabase.database, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(ids.size(), 3);

    {
        MediaRepository repository(testDatabase.database);
        MediaLibraryModel model;
        model.setItems(repository.fetchLibraryItems());
        QCOMPARE(model.rowCount(), 3);

        AppController controller(&model, &repository);
        controller.setDatabaseState(true, QStringLiteral("DB ready"), testDatabase.database.databaseName());

        const QMetaObject &metaObject = AppController::staticMetaObject;
        QVERIFY(metaObject.indexOfMethod("selectRelative(int)") >= 0);

        QCOMPARE(controller.selectedIndex(), 0);
        QCOMPARE(controller.selectedMediaCount(), 1);
        QVERIFY(!controller.selectRelative(-1));
        QCOMPARE(controller.selectedIndex(), 0);

        QVERIFY(controller.selectRelative(1));
        QCOMPARE(controller.selectedIndex(), 1);
        QCOMPARE(controller.selectedMediaCount(), 1);
        QVERIFY(model.isSelected(1));

        controller.selectRangeOrToggle(2, false, true);
        QCOMPARE(controller.selectedIndex(), 2);
        QCOMPARE(controller.selectedMediaCount(), 2);
        QVERIFY(model.isSelected(1));
        QVERIFY(model.isSelected(2));

        QVERIFY(controller.selectRelative(-1));
        QCOMPARE(controller.selectedIndex(), 1);
        QCOMPARE(controller.selectedMediaCount(), 1);
        QVERIFY(model.isSelected(1));
        QVERIFY(!model.isSelected(2));

        controller.selectIndex(2);
        QVERIFY(!controller.selectRelative(1));
        QCOMPARE(controller.selectedIndex(), 2);
        QCOMPARE(controller.selectedMediaCount(), 1);
    }

    closeDatabase(&testDatabase);
}

void M27KeyboardReviewModeTest::qmlReviewShortcutLayerIsWired()
{
    const QString mainQml = readTextFile(sourcePath(QStringLiteral("qml/Main.qml")));
    const QString shortcutLayer = readTextFile(sourcePath(QStringLiteral("qml/components/ReviewShortcutLayer.qml")));
    const QString playerPage = readTextFile(sourcePath(QStringLiteral("qml/pages/PlayerPage.qml")));
    const QString roadmap = readTextFile(sourcePath(QStringLiteral("docs/roadmap.md")));
    QVERIFY2(!mainQml.isEmpty(), "Could not read Main.qml");
    QVERIFY2(!shortcutLayer.isEmpty(), "Could not read ReviewShortcutLayer.qml");
    QVERIFY2(!playerPage.isEmpty(), "Could not read PlayerPage.qml");
    QVERIFY2(!roadmap.isEmpty(), "Could not read roadmap.md");

    QVERIFY(mainQml.contains(QStringLiteral("quickReviewMode")));
    QVERIFY(mainQml.contains(QStringLiteral("reviewShortcutModalBlocked")));
    QVERIFY(mainQml.contains(QStringLiteral("ReviewShortcutLayer")));
    QVERIFY(mainQml.contains(QStringLiteral("Review Mode")));
    QVERIFY(mainQml.contains(QStringLiteral("setQuickReviewMode")));

    for (const QString &sequence : {
             QStringLiteral("Ctrl+R"),
             QStringLiteral("Up"),
             QStringLiteral("K"),
             QStringLiteral("Down"),
             QStringLiteral("J"),
             QStringLiteral("Space"),
             QStringLiteral("Left"),
             QStringLiteral("H"),
             QStringLiteral("Right"),
             QStringLiteral("L"),
             QStringLiteral("0"),
             QStringLiteral("1"),
             QStringLiteral("2"),
             QStringLiteral("3"),
             QStringLiteral("4"),
             QStringLiteral("5"),
             QStringLiteral("U"),
             QStringLiteral("R"),
             QStringLiteral("N"),
             QStringLiteral("F"),
             QStringLiteral("D"),
             QStringLiteral("S"),
         }) {
        QVERIFY2(shortcutLayer.contains(QStringLiteral("sequence: \"%1\"").arg(sequence)), qPrintable(sequence));
    }

    QVERIFY(shortcutLayer.contains(QStringLiteral("selectRelative(offset)")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("moveSelection(-1)")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("moveSelection(1)")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("persistPlaybackPosition()")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("setSelectedMediaRating")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("setSelectedMediaReviewStatus")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("setSelectedFavorite")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("setSelectedDeleteCandidate")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("captureSelectedSnapshot")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("focusItemBlocksShortcuts")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("modalBlocked")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("unreviewed")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("reviewed")));
    QVERIFY(shortcutLayer.contains(QStringLiteral("needs_followup")));

    QVERIFY(playerPage.contains(QStringLiteral("reviewShortcutsActive")));
    QVERIFY(playerPage.contains(QStringLiteral("!root.reviewShortcutsActive")));

    QVERIFY(!roadmap.contains(QStringLiteral("## M25")));
    QVERIFY(!roadmap.contains(QStringLiteral("M25 중복 파일 탐지")));
    QVERIFY(roadmap.contains(QStringLiteral("## M27. 키보드 중심 검토 모드")));
    QVERIFY(roadmap.contains(QStringLiteral("| 검토 모드 토글 | Ctrl+R |")));
}

QTEST_MAIN(M27KeyboardReviewModeTest)

#include "M27KeyboardReviewModeTest.moc"

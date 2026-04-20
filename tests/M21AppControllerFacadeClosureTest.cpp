#include "app/AppController.h"
#include "media/MediaLibraryModel.h"

#include <QFile>
#include <QMetaMethod>
#include <QRegularExpression>
#include <QtTest/QtTest>

class M21AppControllerFacadeClosureTest : public QObject
{
    Q_OBJECT

private slots:
    void qmlFacingApiRemainsStableAfterFacadeSplit();
    void appControllerSourceStaysThinAndFreeOfDirectInfrastructure();
};

namespace {
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
}

void M21AppControllerFacadeClosureTest::qmlFacingApiRemainsStableAfterFacadeSplit()
{
    const QMetaObject &metaObject = AppController::staticMetaObject;

    for (const char *propertyName : {
             "selectedIndex",
             "selectedMedia",
             "databaseReady",
             "databaseStatus",
             "scanInProgress",
             "scanStatus",
             "librarySearchText",
             "librarySortKey",
             "librarySortAscending",
             "showThumbnails",
             "metadataInProgress",
             "detailStatus",
             "fileActionStatus",
             "snapshotInProgress",
             "thumbnailMaintenanceInProgress",
             "ffprobePath",
             "ffmpegPath",
             "settingsStatus",
             "toolsStatus",
             "playerVolume",
             "playerMuted",
             "workEvents",
         }) {
        QVERIFY2(metaObject.indexOfProperty(propertyName) >= 0, propertyName);
    }

    for (const char *methodName : {
             "selectIndex",
             "startDirectoryScan",
             "rescanCurrentRoot",
             "cancelScan",
             "refreshSelectedMetadata",
             "saveSelectedMediaDetails",
             "renameSelectedMedia",
             "setSelectedFavorite",
             "setSelectedDeleteCandidate",
             "saveSelectedPlaybackPosition",
             "captureSelectedSnapshot",
             "rebuildThumbnailCache",
             "resetLibrary",
             "aboutInfo",
             "systemInfo",
             "saveSettings",
             "resetToolPathsToPath",
             "validateExternalTools",
             "openLogFile",
             "openLogFolder",
             "openAppDataFolder",
             "diagnosticReport",
             "clearWorkEvents",
             "localPathFromUrl",
         }) {
        bool found = false;
        for (int index = metaObject.methodOffset(); index < metaObject.methodCount(); ++index) {
            if (metaObject.method(index).name() == methodName) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, methodName);
    }

    MediaLibraryModel model;
    QVERIFY(model.roleNames().values().contains("thumbnailPath"));
}

void M21AppControllerFacadeClosureTest::appControllerSourceStaysThinAndFreeOfDirectInfrastructure()
{
    const QString source = readTextFile(sourcePath(QStringLiteral("src/app/AppController.cpp")));
    const QString header = readTextFile(sourcePath(QStringLiteral("src/app/AppController.h")));
    QVERIFY2(!source.isEmpty(), "Could not read AppController.cpp");
    QVERIFY2(!header.isEmpty(), "Could not read AppController.h");

    const int sourceLines = source.count(QLatin1Char('\n')) + (source.isEmpty() ? 0 : 1);
    QVERIFY2(sourceLines <= 500, qPrintable(QStringLiteral("AppController.cpp has %1 lines").arg(sourceLines)));

    const QRegularExpression forbidden(QStringLiteral(R"(QDesktopServices|QTextStream|ExternalToolService|AppSettingsRepository|#include\s*\"db/MediaRepository|QFutureWatcher|QtConcurrent)"));
    QVERIFY2(!source.contains(forbidden), "AppController.cpp has a direct infrastructure dependency");
    QVERIFY2(!header.contains(forbidden), "AppController.h has a direct infrastructure dependency");
}

QTEST_MAIN(M21AppControllerFacadeClosureTest)

#include "M21AppControllerFacadeClosureTest.moc"

#include "app/AppController.h"
#include "media/MediaLibraryModel.h"

#include <QMetaMethod>
#include <QtTest/QtTest>

class M17PresentationFacadeTest : public QObject
{
    Q_OBJECT

private slots:
    void appControllerKeepsQmlFacingApiStable();
    void mediaLibraryModelKeepsRoleNamesStable();
};

void M17PresentationFacadeTest::appControllerKeepsQmlFacingApiStable()
{
    const QMetaObject &metaObject = AppController::staticMetaObject;

    for (const char *propertyName : {
             "selectedIndex",
             "selectedMedia",
             "databaseReady",
             "scanInProgress",
             "librarySearchText",
             "librarySortKey",
             "metadataInProgress",
             "snapshotInProgress",
             "thumbnailMaintenanceInProgress",
             "settingsStatus",
         }) {
        QVERIFY2(metaObject.indexOfProperty(propertyName) >= 0, propertyName);
    }

    for (const char *methodName : {
             "startDirectoryScan",
             "rescanCurrentRoot",
             "saveSelectedMediaDetails",
             "renameSelectedMedia",
             "captureSelectedSnapshot",
             "rebuildThumbnailCache",
             "saveSettings",
             "diagnosticReport",
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
}

void M17PresentationFacadeTest::mediaLibraryModelKeepsRoleNamesStable()
{
    MediaLibraryModel model;
    const QHash<int, QByteArray> roles = model.roleNames();
    QVERIFY(roles.values().contains("id"));
    QVERIFY(roles.values().contains("fileName"));
    QVERIFY(roles.values().contains("thumbnailPath"));
    QVERIFY(roles.values().contains("isFavorite"));
    QVERIFY(roles.values().contains("lastPositionMs"));
}

QTEST_MAIN(M17PresentationFacadeTest)

#include "M17PresentationFacadeTest.moc"

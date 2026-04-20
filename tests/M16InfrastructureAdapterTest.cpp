#include "infrastructure/PathSecurityPolicy.h"
#include "media/MetadataService.h"
#include "media/RenameService.h"
#include "media/ScanService.h"
#include "media/SnapshotService.h"
#include "media/ThumbnailService.h"
#include "ports/IFileRenamer.h"
#include "ports/IFileScanner.h"
#include "ports/IMetadataProbe.h"
#include "ports/IPathPolicy.h"
#include "ports/ISnapshotGenerator.h"
#include "ports/IThumbnailGenerator.h"

#include <type_traits>

#include <QTemporaryDir>
#include <QtTest/QtTest>

class M16InfrastructureAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void adaptersImplementApplicationPorts();
    void pathPolicyDelegatesToSharedPathSecurity();
};

void M16InfrastructureAdapterTest::adaptersImplementApplicationPorts()
{
    QVERIFY((std::is_base_of_v<IFileScanner, ScanService>));
    QVERIFY((std::is_base_of_v<IFileRenamer, RenameService>));
    QVERIFY((std::is_base_of_v<IMetadataProbe, MetadataService>));
    QVERIFY((std::is_base_of_v<ISnapshotGenerator, SnapshotService>));
    QVERIFY((std::is_base_of_v<IThumbnailGenerator, ThumbnailService>));
}

void M16InfrastructureAdapterTest::pathPolicyDelegatesToSharedPathSecurity()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    PathSecurityPolicy policy;
    QVERIFY(!policy.appDataPath().isEmpty());
    QVERIFY(policy.samePath(tempDir.path(), tempDir.path()));
    QVERIFY(policy.isInsideOrEqual(tempDir.filePath(QStringLiteral("child")), tempDir.path()));
    QVERIFY(!policy.redactedLocalPath(tempDir.filePath(QStringLiteral("secret.mp4"))).contains(tempDir.path()));
}

QTEST_MAIN(M16InfrastructureAdapterTest)

#include "M16InfrastructureAdapterTest.moc"

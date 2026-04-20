#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QtTest/QtTest>

class M20ArchitectureBoundaryClosureTest : public QObject
{
    Q_OBJECT

private slots:
    void portsNoLongerDependOnCore();
    void sourceFoldersRespectClosedLayerIncludes();
    void appControllerDoesNotUseClosedConcreteDependencies();
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

QStringList filesUnder(const QString &relativeFolder)
{
    QStringList files;
    QDirIterator iterator(sourcePath(relativeFolder), {QStringLiteral("*.h"), QStringLiteral("*.cpp")}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        files.append(iterator.next());
    }
    return files;
}

void verifyNoPatternInFiles(const QStringList &files, const QRegularExpression &pattern)
{
    for (const QString &path : files) {
        const QString text = readTextFile(path);
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("Could not read %1").arg(path)));
        QVERIFY2(!text.contains(pattern), qPrintable(QStringLiteral("Forbidden include/dependency in %1").arg(path)));
    }
}
}

void M20ArchitectureBoundaryClosureTest::portsNoLongerDependOnCore()
{
    const QString cmake = readTextFile(sourcePath(QStringLiteral("CMakeLists.txt")));
    QVERIFY2(!cmake.isEmpty(), "Could not read CMakeLists.txt");

    const QRegularExpression portsLinkBlock(QStringLiteral(R"(target_link_libraries\s*\(\s*pickle_ports\s+INTERFACE([\s\S]*?)\))"));
    const QRegularExpressionMatch match = portsLinkBlock.match(cmake);
    QVERIFY2(match.hasMatch(), "Could not find pickle_ports link block");
    QVERIFY2(!match.captured(1).contains(QStringLiteral("pickle_core")), "pickle_ports must not link pickle_core");

    verifyNoPatternInFiles(filesUnder(QStringLiteral("src/ports")), QRegularExpression(QStringLiteral(R"(#include\s*[<\"]core/)")));
}

void M20ArchitectureBoundaryClosureTest::sourceFoldersRespectClosedLayerIncludes()
{
    verifyNoPatternInFiles(
        filesUnder(QStringLiteral("src/domain")),
        QRegularExpression(QStringLiteral(R"(#include\s*[<\"]((Q(Sql|Process|Image|AbstractListModel|VariantMap|Qml|Quick))|((app|core|db|media)/))")));

    verifyNoPatternInFiles(
        filesUnder(QStringLiteral("src/application")),
        QRegularExpression(QStringLiteral(R"(#include\s*[<\"]((app|db|qml|ui)/|Q(Sql|AbstractListModel|Qml|Quick)))")));

    verifyNoPatternInFiles(
        filesUnder(QStringLiteral("src/ports")),
        QRegularExpression(QStringLiteral(R"(#include\s*[<\"]core/)")));
}

void M20ArchitectureBoundaryClosureTest::appControllerDoesNotUseClosedConcreteDependencies()
{
    const QString controllerHeader = readTextFile(sourcePath(QStringLiteral("src/app/AppController.h")));
    const QString controllerSource = readTextFile(sourcePath(QStringLiteral("src/app/AppController.cpp")));
    QVERIFY2(!controllerHeader.isEmpty(), "Could not read AppController.h");
    QVERIFY2(!controllerSource.isEmpty(), "Could not read AppController.cpp");

    const QRegularExpression forbidden(QStringLiteral(R"(QDesktopServices|QTextStream|ExternalToolService|AppSettingsRepository|#include\s*\"db/MediaRepository|QFutureWatcher|QtConcurrent)"));
    QVERIFY2(!controllerHeader.contains(forbidden), "AppController.h keeps a closed concrete dependency");
    QVERIFY2(!controllerSource.contains(forbidden), "AppController.cpp keeps a closed concrete dependency");
}

QTEST_MAIN(M20ArchitectureBoundaryClosureTest)

#include "M20ArchitectureBoundaryClosureTest.moc"

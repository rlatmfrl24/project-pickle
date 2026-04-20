#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QtTest/QtTest>

class M13ArchitectureBoundaryTest : public QObject
{
    Q_OBJECT

private slots:
    void cmakeDeclaresCleanArchitectureTargets();
    void domainAndApplicationAvoidForbiddenFrameworkIncludes();
};

namespace {
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString sourcePath(const QString &relativePath)
{
    return QStringLiteral(PICKLE_SOURCE_DIR) + QLatin1Char('/') + relativePath;
}
}

void M13ArchitectureBoundaryTest::cmakeDeclaresCleanArchitectureTargets()
{
    const QString cmake = readTextFile(sourcePath(QStringLiteral("CMakeLists.txt")));
    QVERIFY2(!cmake.isEmpty(), "Could not read CMakeLists.txt");

    for (const QString &target : {
             QStringLiteral("pickle_domain"),
             QStringLiteral("pickle_ports"),
             QStringLiteral("pickle_application"),
             QStringLiteral("pickle_infrastructure"),
             QStringLiteral("pickle_presentation"),
         }) {
        QVERIFY2(cmake.contains(target), qPrintable(QStringLiteral("Missing architecture target %1").arg(target)));
    }
}

void M13ArchitectureBoundaryTest::domainAndApplicationAvoidForbiddenFrameworkIncludes()
{
    const QStringList files = {
        sourcePath(QStringLiteral("src/domain/MediaEntities.h")),
        sourcePath(QStringLiteral("src/domain/LibraryQuery.h")),
        sourcePath(QStringLiteral("src/domain/OperationResult.h")),
        sourcePath(QStringLiteral("src/application/LoadLibraryUseCase.h")),
        sourcePath(QStringLiteral("src/application/LoadLibraryUseCase.cpp")),
        sourcePath(QStringLiteral("src/application/ScanLibraryUseCase.h")),
        sourcePath(QStringLiteral("src/application/ScanLibraryUseCase.cpp")),
        sourcePath(QStringLiteral("src/application/RenameMediaUseCase.h")),
        sourcePath(QStringLiteral("src/application/RenameMediaUseCase.cpp")),
    };
    const QRegularExpression forbidden(QStringLiteral(R"(#include\s*<Q(Sql|Process|Image|AbstractListModel|VariantMap|Qml|Quick))"));

    for (const QString &filePath : files) {
        const QString contents = readTextFile(filePath);
        QVERIFY2(!contents.isEmpty(), qPrintable(QStringLiteral("Could not read %1").arg(filePath)));
        QVERIFY2(!contents.contains(forbidden), qPrintable(QStringLiteral("Forbidden framework include in %1").arg(filePath)));
    }
}

QTEST_MAIN(M13ArchitectureBoundaryTest)

#include "M13ArchitectureBoundaryTest.moc"

#include "TestSupport.h"

#include "db/DatabaseService.h"
#include "db/MigrationService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace PickleTest {

TestDatabase createDatabase(const QString &databasePath)
{
    TestDatabase result;
    result.connectionName = QStringLiteral("pickle_test_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    result.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), result.connectionName);
    result.database.setDatabaseName(databasePath);
    return result;
}

bool initializeMigratedDatabase(TestDatabase *database, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!database) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Test database is unavailable.");
        }
        return false;
    }

    if (!database->database.open()) {
        if (errorMessage) {
            *errorMessage = database->database.lastError().text();
        }
        return false;
    }

    if (!DatabaseService::applyPragmas(database->database, errorMessage)) {
        return false;
    }

    MigrationService migrationService(database->database);
    if (!migrationService.migrate()) {
        if (errorMessage) {
            *errorMessage = migrationService.lastError();
        }
        return false;
    }

    return true;
}

void closeDatabase(TestDatabase *database)
{
    if (!database) {
        return;
    }

    const QString connectionName = database->connectionName;
    if (database->database.isValid()) {
        database->database.close();
        database->database = {};
    }
    database->connectionName.clear();

    if (!connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool writeFile(const QString &filePath, const QByteArray &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    return file.write(contents) == contents.size();
}

ScannedMediaFile scannedMediaFile(const QString &filePath, qint64 fileSize, const QString &modifiedAt)
{
    const QFileInfo fileInfo(filePath);
    ScannedMediaFile file;
    file.fileName = fileInfo.fileName();
    file.filePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    file.fileExtension = fileInfo.suffix().isEmpty()
        ? QString()
        : QStringLiteral(".%1").arg(fileInfo.suffix().toLower());
    file.fileSize = fileSize;
    file.modifiedAt = modifiedAt;
    return file;
}

int insertMediaFile(QSqlDatabase database, const QString &filePath, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const ScannedMediaFile file = scannedMediaFile(filePath);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO media_files (
            file_name,
            file_path,
            file_extension,
            file_size,
            modified_at,
            description
        )
        VALUES (?, ?, ?, ?, ?, '')
    )sql"));
    query.addBindValue(file.fileName);
    query.addBindValue(file.filePath);
    query.addBindValue(file.fileExtension);
    query.addBindValue(file.fileSize);
    query.addBindValue(file.modifiedAt);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }

    return query.lastInsertId().toInt();
}

}

#pragma once

#include "media/MediaTypes.h"

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>

namespace PickleTest {

struct TestDatabase {
    QSqlDatabase database;
    QString connectionName;
};

TestDatabase createDatabase(const QString &databasePath);
bool initializeMigratedDatabase(TestDatabase *database, QString *errorMessage = nullptr);
void closeDatabase(TestDatabase *database);
bool writeFile(const QString &filePath, const QByteArray &contents = QByteArray("pickle"));
ScannedMediaFile scannedMediaFile(
    const QString &filePath,
    qint64 fileSize = 1024,
    const QString &modifiedAt = QStringLiteral("2026-04-20T00:00:00.000Z"));
int insertMediaFile(QSqlDatabase database, const QString &filePath, QString *errorMessage = nullptr);

}

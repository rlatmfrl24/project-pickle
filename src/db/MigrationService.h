#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

class MigrationService
{
public:
    explicit MigrationService(QSqlDatabase database);

    bool migrate();
    QString lastError() const;

private:
    bool ensureMigrationTable();
    bool hasMigration(const QString &version, bool *ok);
    bool applyMigration(const QString &version, const QStringList &statements);
    bool recordMigration(const QString &version);
    bool execute(const QString &statement);

    QSqlDatabase m_database;
    QString m_lastError;
};

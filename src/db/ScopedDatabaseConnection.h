#pragma once

#include <QSqlDatabase>
#include <QString>

class ScopedDatabaseConnection
{
public:
    ScopedDatabaseConnection(const QString &databasePath, const QString &connectionNamePrefix);
    ~ScopedDatabaseConnection();

    ScopedDatabaseConnection(const ScopedDatabaseConnection &) = delete;
    ScopedDatabaseConnection &operator=(const ScopedDatabaseConnection &) = delete;

    QSqlDatabase database() const;
    QString connectionName() const;
    QString lastError() const;
    bool isOpen() const;

private:
    QString m_connectionName;
    QString m_lastError;
    QSqlDatabase m_database;
};

#include "ScopedDatabaseConnection.h"

#include "db/DatabaseService.h"

#include <QUuid>

ScopedDatabaseConnection::ScopedDatabaseConnection(const QString &databasePath, const QString &connectionNamePrefix)
    : m_connectionName(QStringLiteral("%1_%2")
                           .arg(connectionNamePrefix, QUuid::createUuid().toString(QUuid::Id128)))
{
    m_database = DatabaseService::openNamedConnection(databasePath, m_connectionName, &m_lastError);
}

ScopedDatabaseConnection::~ScopedDatabaseConnection()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = {};
    }

    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QSqlDatabase ScopedDatabaseConnection::database() const
{
    return m_database;
}

QString ScopedDatabaseConnection::connectionName() const
{
    return m_connectionName;
}

QString ScopedDatabaseConnection::lastError() const
{
    return m_lastError;
}

bool ScopedDatabaseConnection::isOpen() const
{
    return m_database.isOpen();
}

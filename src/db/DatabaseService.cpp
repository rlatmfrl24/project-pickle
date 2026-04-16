#include "DatabaseService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <utility>

DatabaseService::DatabaseService(QString connectionName)
    : m_connectionName(std::move(connectionName))
    , m_databasePath(defaultDatabasePath())
{
}

DatabaseService::~DatabaseService()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = {};
    }

    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseService::open()
{
    const QFileInfo databaseFileInfo(m_databasePath);
    QDir databaseDir(databaseFileInfo.absolutePath());
    if (!databaseDir.exists() && !databaseDir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("Failed to create database directory: %1")
                          .arg(databaseDir.absolutePath());
        return false;
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase::database(m_connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    }

    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    QSqlQuery pragmaQuery(m_database);
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        m_lastError = pragmaQuery.lastError().text();
        return false;
    }

    m_lastError.clear();
    return true;
}

QSqlDatabase DatabaseService::database() const
{
    return m_database;
}

QString DatabaseService::connectionName() const
{
    return m_connectionName;
}

QString DatabaseService::databasePath() const
{
    return m_databasePath;
}

QString DatabaseService::lastError() const
{
    return m_lastError;
}

bool DatabaseService::isOpen() const
{
    return m_database.isOpen();
}

QString DatabaseService::defaultDatabasePath()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QCoreApplication::applicationDirPath();
    }

    return QDir(basePath).filePath(QStringLiteral("pickle.sqlite3"));
}

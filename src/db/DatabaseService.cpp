#include "DatabaseService.h"

#include "core/PathSecurity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>

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
    m_database = openNamedConnection(m_databasePath, m_connectionName, &m_lastError);
    if (!m_database.isOpen()) {
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

QSqlDatabase DatabaseService::openNamedConnection(
    const QString &databasePath,
    const QString &connectionName,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    if (databasePath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Database path is empty.");
        }
        return {};
    }

    const QFileInfo databaseFileInfo(databasePath);
    QDir databaseDir(databaseFileInfo.absolutePath());
    if (!databaseDir.exists() && !databaseDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create database directory: %1")
                                .arg(databaseDir.absolutePath());
        }
        return {};
    }

    QSqlDatabase database = QSqlDatabase::contains(connectionName)
        ? QSqlDatabase::database(connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(databasePath);

    if (!database.open()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return {};
    }

    if (!applyPragmas(database, errorMessage)) {
        database.close();
        return {};
    }

    return database;
}

bool DatabaseService::applyPragmas(QSqlDatabase database, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    if (!database.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Database connection is not open.");
        }
        return false;
    }

    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("PRAGMA busy_timeout = 5000"),
        QStringLiteral("PRAGMA temp_store = MEMORY"),
    };

    for (const QString &pragma : pragmas) {
        QSqlQuery query(database);
        if (!query.exec(pragma)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 | SQL: %2")
                                    .arg(query.lastError().text(), pragma);
            }
            return false;
        }
    }

    return true;
}

QString DatabaseService::defaultDatabasePath()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QCoreApplication::applicationDirPath();
    }

    return QDir(basePath.isEmpty() ? PathSecurity::appDataPath() : basePath).filePath(QStringLiteral("pickle.sqlite3"));
}

#include "MigrationService.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <utility>

namespace {
constexpr auto InitialSchemaVersion = "001_initial_schema";
}

MigrationService::MigrationService(QSqlDatabase database)
    : m_database(std::move(database))
{
}

bool MigrationService::migrate()
{
    if (!m_database.isOpen()) {
        m_lastError = QStringLiteral("Database connection is not open.");
        return false;
    }

    if (!ensureMigrationTable()) {
        return false;
    }

    const QString version = QString::fromLatin1(InitialSchemaVersion);
    bool migrationLookupOk = false;
    if (hasMigration(version, &migrationLookupOk)) {
        return true;
    }
    if (!migrationLookupOk) {
        return false;
    }

    const QStringList initialSchemaStatements = {
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS media_files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_name TEXT NOT NULL,
                file_path TEXT NOT NULL UNIQUE,
                file_extension TEXT,
                file_size INTEGER NOT NULL DEFAULT 0,
                modified_at TEXT,
                hash_value TEXT,
                duration_ms INTEGER,
                bitrate INTEGER,
                frame_rate REAL,
                width INTEGER,
                height INTEGER,
                video_codec TEXT,
                audio_codec TEXT,
                description TEXT NOT NULL DEFAULT '',
                review_status TEXT NOT NULL DEFAULT 'unreviewed',
                rating INTEGER NOT NULL DEFAULT 0 CHECK (rating >= 0 AND rating <= 5),
                thumbnail_path TEXT,
                last_played_at TEXT,
                last_position_ms INTEGER NOT NULL DEFAULT 0,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                updated_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                color TEXT,
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS media_tags (
                media_id INTEGER NOT NULL,
                tag_id INTEGER NOT NULL,
                PRIMARY KEY (media_id, tag_id),
                FOREIGN KEY (media_id) REFERENCES media_files(id) ON DELETE CASCADE,
                FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS snapshots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                media_id INTEGER NOT NULL,
                image_path TEXT NOT NULL,
                timestamp_ms INTEGER NOT NULL DEFAULT 0,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                FOREIGN KEY (media_id) REFERENCES media_files(id) ON DELETE CASCADE
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS scan_roots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                root_path TEXT NOT NULL UNIQUE,
                is_enabled INTEGER NOT NULL DEFAULT 1,
                last_scanned_at TEXT,
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql"),
        QStringLiteral(R"sql(
            CREATE TABLE IF NOT EXISTS app_settings (
                key TEXT PRIMARY KEY,
                value TEXT,
                updated_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )sql"),
    };

    return applyMigration(version, initialSchemaStatements);
}

QString MigrationService::lastError() const
{
    return m_lastError;
}

bool MigrationService::ensureMigrationTable()
{
    return execute(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            version TEXT NOT NULL UNIQUE,
            applied_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )sql"));
}

bool MigrationService::hasMigration(const QString &version, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT 1 FROM schema_migrations WHERE version = ? LIMIT 1"));
    query.addBindValue(version);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (ok) {
        *ok = true;
    }

    return query.next();
}

bool MigrationService::applyMigration(const QString &version, const QStringList &statements)
{
    if (!m_database.transaction()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    for (const QString &statement : statements) {
        if (!execute(statement)) {
            m_database.rollback();
            return false;
        }
    }

    if (!recordMigration(version)) {
        m_database.rollback();
        return false;
    }

    if (!m_database.commit()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    return true;
}

bool MigrationService::recordMigration(const QString &version)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO schema_migrations (version) VALUES (?)"));
    query.addBindValue(version);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool MigrationService::execute(const QString &statement)
{
    QSqlQuery query(m_database);
    if (!query.exec(statement)) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseService
{
public:
    explicit DatabaseService(QString connectionName = QStringLiteral("pickle_main"));
    ~DatabaseService();

    bool open();

    QSqlDatabase database() const;
    QString connectionName() const;
    QString databasePath() const;
    QString lastError() const;
    bool isOpen() const;

    static QSqlDatabase openNamedConnection(
        const QString &databasePath,
        const QString &connectionName,
        QString *errorMessage = nullptr);
    static bool applyPragmas(QSqlDatabase database, QString *errorMessage = nullptr);

private:
    static QString defaultDatabasePath();

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_database;
};

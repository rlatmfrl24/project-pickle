#pragma once

#include "media/MediaTypes.h"

#include <QSqlDatabase>
#include <QString>

class AppSettingsRepository
{
public:
    explicit AppSettingsRepository(QSqlDatabase database);

    AppSettings load();
    bool save(const AppSettings &settings);
    QString lastError() const;

    static AppSettings normalized(AppSettings settings);

private:
    bool ensureOpen();
    bool saveValue(const QString &key, const QString &value);
    void setLastError(const QString &error);

    QSqlDatabase m_database;
    QString m_lastError;
};

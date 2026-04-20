#pragma once

#include "media/MediaTypes.h"
#include "ports/ISettingsRepository.h"

#include <QSqlDatabase>
#include <QString>

class AppSettingsRepository : public ISettingsRepository
{
public:
    explicit AppSettingsRepository(QSqlDatabase database);

    AppSettings load() override;
    bool save(const AppSettings &settings) override;
    QString lastError() const override;

    static AppSettings normalized(AppSettings settings);

private:
    bool ensureOpen();
    bool saveValue(const QString &key, const QString &value);
    void setLastError(const QString &error);

    QSqlDatabase m_database;
    QString m_lastError;
};

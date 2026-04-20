#include "AppSettingsRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVector>

#include <algorithm>
#include <utility>

namespace {
constexpr auto FfprobePathKey = "tools.ffprobePath";
constexpr auto FfmpegPathKey = "tools.ffmpegPath";
constexpr auto ShowThumbnailsKey = "library.showThumbnails";
constexpr auto SortKeyKey = "library.sortKey";
constexpr auto SortAscendingKey = "library.sortAscending";
constexpr auto LastOpenFolderKey = "library.lastOpenFolder";
constexpr auto PlayerVolumeKey = "player.volume";
constexpr auto PlayerMutedKey = "player.muted";

bool parseBool(const QString &value, bool fallback)
{
    const QString normalizedValue = value.trimmed().toLower();
    if (normalizedValue == QStringLiteral("1") || normalizedValue == QStringLiteral("true")) {
        return true;
    }
    if (normalizedValue == QStringLiteral("0") || normalizedValue == QStringLiteral("false")) {
        return false;
    }

    return fallback;
}

QString boolText(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}
}

AppSettingsRepository::AppSettingsRepository(QSqlDatabase database)
    : m_database(std::move(database))
{
}

AppSettings AppSettingsRepository::load()
{
    AppSettings settings;
    if (!ensureOpen()) {
        return settings;
    }

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT key, value FROM app_settings"))) {
        setLastError(query.lastError().text());
        return settings;
    }

    while (query.next()) {
        const QString key = query.value(0).toString();
        const QString value = query.value(1).toString();
        if (key == QString::fromLatin1(FfprobePathKey)) {
            settings.ffprobePath = value.trimmed();
        } else if (key == QString::fromLatin1(FfmpegPathKey)) {
            settings.ffmpegPath = value.trimmed();
        } else if (key == QString::fromLatin1(ShowThumbnailsKey)) {
            settings.showThumbnails = parseBool(value, settings.showThumbnails);
        } else if (key == QString::fromLatin1(SortKeyKey)) {
            settings.sortKey = value;
        } else if (key == QString::fromLatin1(SortAscendingKey)) {
            settings.sortAscending = parseBool(value, settings.sortAscending);
        } else if (key == QString::fromLatin1(LastOpenFolderKey)) {
            settings.lastOpenFolder = value.trimmed();
        } else if (key == QString::fromLatin1(PlayerVolumeKey)) {
            bool ok = false;
            const double volume = value.toDouble(&ok);
            if (ok) {
                settings.playerVolume = volume;
            }
        } else if (key == QString::fromLatin1(PlayerMutedKey)) {
            settings.playerMuted = parseBool(value, settings.playerMuted);
        }
    }

    m_lastError.clear();
    return normalized(settings);
}

bool AppSettingsRepository::save(const AppSettings &settings)
{
    if (!ensureOpen()) {
        return false;
    }

    const AppSettings normalizedSettings = normalized(settings);
    if (!m_database.transaction()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    const QVector<std::pair<QString, QString>> values = {
        {QString::fromLatin1(FfprobePathKey), normalizedSettings.ffprobePath},
        {QString::fromLatin1(FfmpegPathKey), normalizedSettings.ffmpegPath},
        {QString::fromLatin1(ShowThumbnailsKey), boolText(normalizedSettings.showThumbnails)},
        {QString::fromLatin1(SortKeyKey), normalizedSettings.sortKey},
        {QString::fromLatin1(SortAscendingKey), boolText(normalizedSettings.sortAscending)},
        {QString::fromLatin1(LastOpenFolderKey), normalizedSettings.lastOpenFolder},
        {QString::fromLatin1(PlayerVolumeKey), QString::number(normalizedSettings.playerVolume, 'f', 3)},
        {QString::fromLatin1(PlayerMutedKey), boolText(normalizedSettings.playerMuted)},
    };

    for (const auto &[key, value] : values) {
        if (!saveValue(key, value)) {
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        setLastError(m_database.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

QString AppSettingsRepository::lastError() const
{
    return m_lastError;
}

AppSettings AppSettingsRepository::normalized(AppSettings settings)
{
    settings.ffprobePath = settings.ffprobePath.trimmed();
    settings.ffmpegPath = settings.ffmpegPath.trimmed();
    settings.lastOpenFolder = settings.lastOpenFolder.trimmed();

    const QString normalizedSortKey = settings.sortKey.trimmed().toLower();
    if (normalizedSortKey == QStringLiteral("size") || normalizedSortKey == QStringLiteral("modified")) {
        settings.sortKey = normalizedSortKey;
    } else {
        settings.sortKey = QStringLiteral("name");
    }

    settings.playerVolume = std::clamp(settings.playerVolume, 0.0, 1.0);
    return settings;
}

bool AppSettingsRepository::ensureOpen()
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        setLastError(QStringLiteral("Database connection is not open."));
        return false;
    }

    return true;
}

bool AppSettingsRepository::saveValue(const QString &key, const QString &value)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"sql(
        INSERT INTO app_settings (key, value, updated_at)
        VALUES (?, ?, datetime('now'))
        ON CONFLICT(key) DO UPDATE SET
            value = excluded.value,
            updated_at = datetime('now')
    )sql"));
    query.addBindValue(key);
    query.addBindValue(value);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

void AppSettingsRepository::setLastError(const QString &error)
{
    m_lastError = error;
}

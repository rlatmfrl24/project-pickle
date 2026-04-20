#include "domain/AppSettingsPolicy.h"

#include <algorithm>

namespace AppSettingsPolicy {

AppSettings normalized(AppSettings settings)
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

}

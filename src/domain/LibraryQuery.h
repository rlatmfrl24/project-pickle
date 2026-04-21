#pragma once

#include <QString>

enum class MediaLibrarySortKey {
    Name,
    Size,
    Modified,
    LastPlayed
};

enum class MediaLibraryViewMode {
    All,
    Unreviewed,
    Favorites,
    DeleteCandidates,
    Recent
};

struct MediaLibraryQuery {
    QString searchText;
    QString tagFilter;
    MediaLibrarySortKey sortKey = MediaLibrarySortKey::Name;
    MediaLibraryViewMode viewMode = MediaLibraryViewMode::All;
    bool ascending = true;
};

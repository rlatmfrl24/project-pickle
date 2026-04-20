#pragma once

#include <QString>

enum class MediaLibrarySortKey {
    Name,
    Size,
    Modified
};

struct MediaLibraryQuery {
    QString searchText;
    MediaLibrarySortKey sortKey = MediaLibrarySortKey::Name;
    bool ascending = true;
};

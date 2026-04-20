#pragma once

#include "domain/LibraryQuery.h"
#include "domain/MediaEntities.h"

#include <QVector>

struct LibraryLoadResult {
    MediaLibraryQuery query;
    QVector<MediaLibraryItem> items;
    QString errorMessage;
    int generation = 0;
    bool succeeded = false;
};

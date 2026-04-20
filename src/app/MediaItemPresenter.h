#pragma once

#include "domain/MediaEntities.h"

#include <QVector>

class MediaItemPresenter
{
public:
    static MediaLibraryItem present(const MediaFile &file);
    static QVector<MediaLibraryItem> present(const QVector<MediaFile> &files);
};

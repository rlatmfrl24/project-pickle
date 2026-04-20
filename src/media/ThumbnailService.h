#pragma once

#include "core/CancellationToken.h"
#include "media/MediaTypes.h"

#include <QString>

#include <memory>

class ThumbnailService
{
public:
    ThumbnailGenerationResult generate(
        const ThumbnailGenerationRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;

    static QString defaultThumbnailRoot();
    static bool clearThumbnailRoot(const QString &rootPath, QString *errorMessage = nullptr);
    static bool isUsableThumbnail(const QString &thumbnailPath, int maxWidth = 320);
};

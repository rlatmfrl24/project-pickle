#pragma once

#include "domain/CancellationToken.h"
#include "media/MediaTypes.h"
#include "ports/IThumbnailGenerator.h"

#include <QString>

#include <memory>

class ThumbnailService : public IThumbnailGenerator
{
public:
    ThumbnailGenerationResult generate(
        const ThumbnailGenerationRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const override;

    static QString defaultThumbnailRoot();
    static bool clearThumbnailRoot(const QString &rootPath, QString *errorMessage = nullptr);
    static bool isUsableThumbnail(const QString &thumbnailPath, int maxWidth = 320);
    static bool isManagedThumbnailPath(const QString &thumbnailPath);
};

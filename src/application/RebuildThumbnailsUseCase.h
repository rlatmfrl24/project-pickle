#pragma once

#include "domain/MediaEntities.h"

#include <QVector>

#include <memory>

class CancellationToken;
class IThumbnailGenerator;

class RebuildThumbnailsUseCase
{
public:
    explicit RebuildThumbnailsUseCase(IThumbnailGenerator *thumbnailGenerator);

    QVector<ThumbnailGenerationResult> execute(
        const QVector<ThumbnailBackfillItem> &items,
        const QString &outputRootPath,
        int maxWidth,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;

private:
    IThumbnailGenerator *m_thumbnailGenerator = nullptr;
};

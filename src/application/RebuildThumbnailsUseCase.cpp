#include "RebuildThumbnailsUseCase.h"

#include "ports/IThumbnailGenerator.h"

RebuildThumbnailsUseCase::RebuildThumbnailsUseCase(IThumbnailGenerator *thumbnailGenerator)
    : m_thumbnailGenerator(thumbnailGenerator)
{
}

QVector<ThumbnailGenerationResult> RebuildThumbnailsUseCase::execute(
    const QVector<ThumbnailBackfillItem> &items,
    const QString &outputRootPath,
    int maxWidth,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    QVector<ThumbnailGenerationResult> results;
    if (!m_thumbnailGenerator) {
        return results;
    }

    results.reserve(items.size());
    for (const ThumbnailBackfillItem &item : items) {
        if (cancellationToken && cancellationToken->isCancellationRequested()) {
            break;
        }

        ThumbnailGenerationRequest request;
        request.mediaId = item.mediaId;
        request.sourceImagePath = item.sourceImagePath;
        request.outputRootPath = outputRootPath;
        request.maxWidth = maxWidth;
        results.append(m_thumbnailGenerator->generate(request, cancellationToken));
    }

    return results;
}

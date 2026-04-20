#pragma once

#include "domain/CancellationToken.h"
#include "domain/MediaEntities.h"

#include <memory>

class IThumbnailGenerator
{
public:
    virtual ~IThumbnailGenerator() = default;
    virtual ThumbnailGenerationResult generate(
        const ThumbnailGenerationRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const = 0;
};

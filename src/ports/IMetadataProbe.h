#pragma once

#include "core/CancellationToken.h"
#include "domain/MediaEntities.h"

#include <QString>

#include <memory>

class IMetadataProbe
{
public:
    virtual ~IMetadataProbe() = default;
    virtual MetadataExtractionResult extract(
        const QString &filePath,
        const QString &ffprobeProgram,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const = 0;
};

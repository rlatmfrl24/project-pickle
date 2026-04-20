#pragma once

#include "domain/MediaEntities.h"

#include <memory>

class CancellationToken;
class IMetadataProbe;

class ExtractMetadataUseCase
{
public:
    explicit ExtractMetadataUseCase(IMetadataProbe *metadataProbe);

    MetadataExtractionResult execute(
        const QString &filePath,
        const QString &ffprobeProgram,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;

private:
    IMetadataProbe *m_metadataProbe = nullptr;
};

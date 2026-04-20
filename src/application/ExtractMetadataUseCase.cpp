#include "ExtractMetadataUseCase.h"

#include "ports/IMetadataProbe.h"

ExtractMetadataUseCase::ExtractMetadataUseCase(IMetadataProbe *metadataProbe)
    : m_metadataProbe(metadataProbe)
{
}

MetadataExtractionResult ExtractMetadataUseCase::execute(
    const QString &filePath,
    const QString &ffprobeProgram,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    if (!m_metadataProbe) {
        MetadataExtractionResult result;
        result.errorMessage = QStringLiteral("Metadata probe is unavailable.");
        return result;
    }

    return m_metadataProbe->extract(filePath, ffprobeProgram, cancellationToken);
}

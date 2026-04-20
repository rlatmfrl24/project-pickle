#pragma once

#include "core/CancellationToken.h"
#include "media/MediaTypes.h"
#include "ports/IMetadataProbe.h"

#include <QByteArray>
#include <QString>

#include <memory>

class MetadataService : public IMetadataProbe
{
public:
    MetadataExtractionResult extract(
        const QString &filePath,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;
    MetadataExtractionResult extract(
        const QString &filePath,
        const QString &ffprobeProgram,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const override;

    static MetadataExtractionResult parseFfprobeJson(
        const QByteArray &json,
        const QByteArray &errorOutput = {});
};

#pragma once

#include "core/CancellationToken.h"
#include "media/MediaTypes.h"

#include <QByteArray>
#include <QString>

#include <memory>

class MetadataService
{
public:
    MetadataExtractionResult extract(
        const QString &filePath,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;
    MetadataExtractionResult extract(
        const QString &filePath,
        const QString &ffprobeProgram,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;

    static MetadataExtractionResult parseFfprobeJson(
        const QByteArray &json,
        const QByteArray &errorOutput = {});
};

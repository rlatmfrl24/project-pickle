#pragma once

#include "domain/CancellationToken.h"
#include "domain/MediaEntities.h"

#include <functional>
#include <memory>

class IFileScanner
{
public:
    using ProgressCallback = std::function<void(const ScanProgress &)>;

    virtual ~IFileScanner() = default;
    virtual DirectoryScanResult scanDirectory(
        const QString &rootPath,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr,
        const ProgressCallback &progressCallback = {}) const = 0;
};

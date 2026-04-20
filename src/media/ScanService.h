#pragma once

#include "core/CancellationToken.h"
#include "media/MediaTypes.h"

#include <QString>

#include <functional>
#include <memory>

class ScanService
{
public:
    using ProgressCallback = std::function<void(const ScanProgress &)>;

    DirectoryScanResult scanDirectory(
        const QString &rootPath,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr,
        const ProgressCallback &progressCallback = {}) const;

    static bool isSupportedVideoFile(const QString &filePath);
};

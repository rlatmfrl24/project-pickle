#pragma once

#include "media/MediaTypes.h"

#include <QString>

class ScanService
{
public:
    DirectoryScanResult scanDirectory(const QString &rootPath) const;

    static bool isSupportedVideoFile(const QString &filePath);
};

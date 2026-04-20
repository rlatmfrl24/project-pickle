#pragma once

#include "domain/MediaEntities.h"

#include <QString>

class IFileRenamer
{
public:
    virtual ~IFileRenamer() = default;
    virtual FileRenameResult renameFile(const QString &filePath, const QString &newBaseName) const = 0;
};

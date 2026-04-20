#pragma once

#include "media/MediaTypes.h"

#include <QString>

class RenameService
{
public:
    FileRenameResult renameFile(const QString &filePath, const QString &newBaseName) const;
};

#pragma once

#include "media/MediaTypes.h"
#include "ports/IFileRenamer.h"

#include <QString>

class RenameService : public IFileRenamer
{
public:
    FileRenameResult renameFile(const QString &filePath, const QString &newBaseName) const override;
};

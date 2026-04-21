#pragma once

#include "domain/MediaEntities.h"

#include <QString>

class IFileRenamer
{
public:
    virtual ~IFileRenamer() = default;
    virtual FileRenameResult planRename(const QString &, const QString &) const
    {
        FileRenameResult result;
        result.errorMessage = QStringLiteral("Rename preview is unavailable.");
        return result;
    }
    virtual FileRenameResult renameFile(const QString &filePath, const QString &newBaseName) const = 0;
};

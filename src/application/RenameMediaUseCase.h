#pragma once

#include "domain/MediaEntities.h"
#include "domain/OperationResult.h"

class IFileRenamer;
class IMediaRepository;

class RenameMediaUseCase
{
public:
    RenameMediaUseCase(IFileRenamer *renamer, IMediaRepository *repository);

    OperationResult<ScannedMediaFile> execute(
        int mediaId,
        const QString &filePath,
        const QString &newBaseName,
        const QString &rollbackBaseName = {}) const;

private:
    IFileRenamer *m_renamer = nullptr;
    IMediaRepository *m_repository = nullptr;
};

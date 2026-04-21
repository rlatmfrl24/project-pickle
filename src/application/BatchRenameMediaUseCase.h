#pragma once

#include "domain/MediaEntities.h"

class IFileRenamer;
class IMediaRepository;

class BatchRenameMediaUseCase
{
public:
    BatchRenameMediaUseCase(IFileRenamer *renamer, IMediaRepository *repository);

    BatchRenamePreviewResult preview(
        const QVector<MediaLibraryItem> &items,
        const BatchRenameRule &rule) const;
    BatchRenameExecutionResult execute(
        const QVector<MediaLibraryItem> &items,
        const BatchRenameRule &rule) const;

private:
    IFileRenamer *m_renamer = nullptr;
    IMediaRepository *m_repository = nullptr;
};

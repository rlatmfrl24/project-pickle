#pragma once

#include "domain/OperationResult.h"

class IMediaRepository;

enum class MediaFlagKind {
    Favorite,
    DeleteCandidate
};

class UpdateMediaFlagsUseCase
{
public:
    explicit UpdateMediaFlagsUseCase(IMediaRepository *repository);

    VoidResult execute(int mediaId, MediaFlagKind flag, bool enabled) const;

private:
    IMediaRepository *m_repository = nullptr;
};

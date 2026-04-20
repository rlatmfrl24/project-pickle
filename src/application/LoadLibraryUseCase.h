#pragma once

#include "application/LibraryLoadResult.h"

class IMediaRepository;

class LoadLibraryUseCase
{
public:
    explicit LoadLibraryUseCase(IMediaRepository *repository);

    LibraryLoadResult execute(const MediaLibraryQuery &query, int generation = 0) const;

private:
    IMediaRepository *m_repository = nullptr;
};

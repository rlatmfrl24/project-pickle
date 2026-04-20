#include "LoadLibraryUseCase.h"

#include "ports/IMediaRepository.h"

LoadLibraryUseCase::LoadLibraryUseCase(IMediaRepository *repository)
    : m_repository(repository)
{
}

LibraryLoadResult LoadLibraryUseCase::execute(const MediaLibraryQuery &query, int generation) const
{
    LibraryLoadResult result;
    result.query = query;
    result.generation = generation;

    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }

    result.items = m_repository->fetchLibraryItems(query);
    if (!m_repository->lastError().isEmpty()) {
        result.errorMessage = m_repository->lastError();
        return result;
    }

    result.succeeded = true;
    return result;
}

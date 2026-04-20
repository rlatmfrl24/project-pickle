#include "UpdateMediaFlagsUseCase.h"

#include "ports/IMediaRepository.h"

UpdateMediaFlagsUseCase::UpdateMediaFlagsUseCase(IMediaRepository *repository)
    : m_repository(repository)
{
}

VoidResult UpdateMediaFlagsUseCase::execute(int mediaId, MediaFlagKind flag, bool enabled) const
{
    VoidResult result;
    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }

    const bool updated = flag == MediaFlagKind::Favorite
        ? m_repository->setMediaFavorite(mediaId, enabled)
        : m_repository->setMediaDeleteCandidate(mediaId, enabled);
    if (!updated) {
        result.errorMessage = m_repository->lastError();
        return result;
    }

    result.succeeded = true;
    return result;
}

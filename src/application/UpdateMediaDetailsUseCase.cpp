#include "UpdateMediaDetailsUseCase.h"

#include "ports/IMediaRepository.h"

UpdateMediaDetailsUseCase::UpdateMediaDetailsUseCase(IMediaRepository *repository)
    : m_repository(repository)
{
}

VoidResult UpdateMediaDetailsUseCase::execute(
    int mediaId,
    const QString &description,
    const QString &reviewStatus,
    int rating,
    const QStringList &tags) const
{
    VoidResult result;
    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }

    if (!m_repository->updateMediaDetails(mediaId, description, reviewStatus, rating, tags)) {
        result.errorMessage = m_repository->lastError();
        return result;
    }

    result.succeeded = true;
    return result;
}

#include "SavePlaybackPositionUseCase.h"

#include "ports/IMediaRepository.h"

SavePlaybackPositionUseCase::SavePlaybackPositionUseCase(IMediaRepository *repository)
    : m_repository(repository)
{
}

VoidResult SavePlaybackPositionUseCase::execute(int mediaId, qint64 positionMs, const QString &playedAt) const
{
    VoidResult result;
    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }

    if (!m_repository->updatePlaybackPosition(mediaId, positionMs, playedAt)) {
        result.errorMessage = m_repository->lastError();
        return result;
    }

    result.succeeded = true;
    return result;
}

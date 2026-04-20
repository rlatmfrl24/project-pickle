#pragma once

#include "domain/OperationResult.h"

#include <QString>

class IMediaRepository;

class SavePlaybackPositionUseCase
{
public:
    explicit SavePlaybackPositionUseCase(IMediaRepository *repository);

    VoidResult execute(int mediaId, qint64 positionMs, const QString &playedAt) const;

private:
    IMediaRepository *m_repository = nullptr;
};

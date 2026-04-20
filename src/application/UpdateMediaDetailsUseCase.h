#pragma once

#include "domain/OperationResult.h"

#include <QString>
#include <QStringList>

class IMediaRepository;

class UpdateMediaDetailsUseCase
{
public:
    explicit UpdateMediaDetailsUseCase(IMediaRepository *repository);

    VoidResult execute(
        int mediaId,
        const QString &description,
        const QString &reviewStatus,
        int rating,
        const QStringList &tags) const;

private:
    IMediaRepository *m_repository = nullptr;
};

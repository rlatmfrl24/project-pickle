#pragma once

#include "domain/OperationResult.h"

#include <QString>
#include <QStringList>
#include <QVector>

class IMediaRepository;

class BatchUpdateMediaUseCase
{
public:
    explicit BatchUpdateMediaUseCase(IMediaRepository *repository);

    VoidResult addTags(const QVector<int> &mediaIds, const QStringList &tags) const;
    VoidResult removeTags(const QVector<int> &mediaIds, const QStringList &tags) const;
    VoidResult setReviewStatus(const QVector<int> &mediaIds, const QString &reviewStatus) const;
    VoidResult setRating(const QVector<int> &mediaIds, int rating) const;

private:
    VoidResult validateMediaIds(const QVector<int> &mediaIds) const;
    VoidResult validateTags(const QStringList &tags) const;

    IMediaRepository *m_repository = nullptr;
};

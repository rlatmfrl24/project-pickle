#include "BatchUpdateMediaUseCase.h"

#include "ports/IMediaRepository.h"

namespace {
bool isAllowedReviewStatus(const QString &reviewStatus)
{
    return reviewStatus == QStringLiteral("unreviewed")
        || reviewStatus == QStringLiteral("reviewed")
        || reviewStatus == QStringLiteral("needs_followup");
}

bool hasNonEmptyTag(const QStringList &tags)
{
    for (const QString &tag : tags) {
        if (!tag.trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}
}

BatchUpdateMediaUseCase::BatchUpdateMediaUseCase(IMediaRepository *repository)
    : m_repository(repository)
{
}

VoidResult BatchUpdateMediaUseCase::addTags(const QVector<int> &mediaIds, const QStringList &tags) const
{
    VoidResult result = validateMediaIds(mediaIds);
    if (!result.succeeded) {
        return result;
    }
    result = validateTags(tags);
    if (!result.succeeded) {
        return result;
    }
    if (!m_repository->addTagsToMedia(mediaIds, tags)) {
        result.succeeded = false;
        result.errorMessage = m_repository->lastError();
        return result;
    }
    result.succeeded = true;
    result.errorMessage.clear();
    return result;
}

VoidResult BatchUpdateMediaUseCase::removeTags(const QVector<int> &mediaIds, const QStringList &tags) const
{
    VoidResult result = validateMediaIds(mediaIds);
    if (!result.succeeded) {
        return result;
    }
    result = validateTags(tags);
    if (!result.succeeded) {
        return result;
    }
    if (!m_repository->removeTagsFromMedia(mediaIds, tags)) {
        result.succeeded = false;
        result.errorMessage = m_repository->lastError();
        return result;
    }
    result.succeeded = true;
    result.errorMessage.clear();
    return result;
}

VoidResult BatchUpdateMediaUseCase::setReviewStatus(const QVector<int> &mediaIds, const QString &reviewStatus) const
{
    VoidResult result = validateMediaIds(mediaIds);
    if (!result.succeeded) {
        return result;
    }
    if (!isAllowedReviewStatus(reviewStatus)) {
        result.succeeded = false;
        result.errorMessage = QStringLiteral("Invalid review status.");
        return result;
    }
    if (!m_repository->setMediaReviewStatusBatch(mediaIds, reviewStatus)) {
        result.errorMessage = m_repository->lastError();
        return result;
    }
    result.succeeded = true;
    return result;
}

VoidResult BatchUpdateMediaUseCase::setRating(const QVector<int> &mediaIds, int rating) const
{
    VoidResult result = validateMediaIds(mediaIds);
    if (!result.succeeded) {
        return result;
    }
    if (rating < 0 || rating > 5) {
        result.succeeded = false;
        result.errorMessage = QStringLiteral("Rating must be between 0 and 5.");
        return result;
    }
    if (!m_repository->setMediaRatingBatch(mediaIds, rating)) {
        result.errorMessage = m_repository->lastError();
        return result;
    }
    result.succeeded = true;
    return result;
}

VoidResult BatchUpdateMediaUseCase::validateMediaIds(const QVector<int> &mediaIds) const
{
    VoidResult result;
    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }
    for (const int mediaId : mediaIds) {
        if (mediaId > 0) {
            result.succeeded = true;
            return result;
        }
    }
    result.errorMessage = QStringLiteral("Select media items before applying a batch update.");
    return result;
}

VoidResult BatchUpdateMediaUseCase::validateTags(const QStringList &tags) const
{
    VoidResult result;
    if (!hasNonEmptyTag(tags)) {
        result.errorMessage = QStringLiteral("Enter a tag before applying a batch tag update.");
        return result;
    }
    result.succeeded = true;
    return result;
}

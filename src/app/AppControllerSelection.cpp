#include "app/AppController.h"

#include "app/MediaActionsController.h"
#include "media/MediaLibraryModel.h"

#include <algorithm>

namespace {
bool containsMediaId(const QVector<int> &mediaIds, int mediaId)
{
    return std::find(mediaIds.cbegin(), mediaIds.cend(), mediaId) != mediaIds.cend();
}

void appendMediaIdOnce(QVector<int> *mediaIds, int mediaId)
{
    if (mediaId > 0 && mediaIds && !containsMediaId(*mediaIds, mediaId)) {
        mediaIds->append(mediaId);
    }
}

QVariantList mediaIdsToVariantList(const QVector<int> &mediaIds)
{
    QVariantList values;
    values.reserve(mediaIds.size());
    for (const int mediaId : mediaIds) {
        values.append(mediaId);
    }
    return values;
}
}

QVariantList AppController::selectedMediaIds() const { return mediaIdsToVariantList(m_selectedMediaIds); }
int AppController::selectedMediaCount() const { return m_selectedMediaIds.size(); }

void AppController::setSelectedIndex(int selectedIndex)
{
    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) selectedIndex = -1;
    else if (selectedIndex < 0 || selectedIndex >= m_mediaLibraryModel->rowCount()) return;
    const int mediaId = selectedIndex >= 0 ? m_mediaLibraryModel->mediaIdAt(selectedIndex) : -1;
    applySelection(mediaId > 0 ? QVector<int>({mediaId}) : QVector<int> {}, mediaId, mediaId);
}

void AppController::selectIndex(int index) { setSelectedIndex(index); }

void AppController::selectRangeOrToggle(int index, bool toggle, bool range)
{
    if (!m_mediaLibraryModel || index < 0 || index >= m_mediaLibraryModel->rowCount()) return;
    const int mediaId = m_mediaLibraryModel->mediaIdAt(index); QVector<int> nextIds; int nextAnchor = m_selectionAnchorMediaId > 0 ? m_selectionAnchorMediaId : mediaId;
    if (range) {
        const int anchorIndex = m_mediaLibraryModel->indexOfId(nextAnchor); const int start = std::min(anchorIndex >= 0 ? anchorIndex : index, index); const int end = std::max(anchorIndex >= 0 ? anchorIndex : index, index);
        if (toggle) nextIds = m_selectedMediaIds;
        for (int row = start; row <= end; ++row) appendMediaIdOnce(&nextIds, m_mediaLibraryModel->mediaIdAt(row));
    } else if (toggle) {
        nextIds = m_selectedMediaIds;
        if (containsMediaId(nextIds, mediaId)) nextIds.erase(std::remove(nextIds.begin(), nextIds.end(), mediaId), nextIds.end());
        else appendMediaIdOnce(&nextIds, mediaId);
        nextAnchor = mediaId;
    } else {
        nextIds = {mediaId};
        nextAnchor = mediaId;
    }
    applySelection(nextIds, containsMediaId(nextIds, mediaId) ? mediaId : (nextIds.isEmpty() ? -1 : nextIds.first()), nextAnchor);
}

void AppController::selectAllVisible()
{
    QVector<int> ids;
    if (m_mediaLibraryModel) for (int row = 0; row < m_mediaLibraryModel->rowCount(); ++row) appendMediaIdOnce(&ids, m_mediaLibraryModel->mediaIdAt(row));
    applySelection(ids, containsMediaId(ids, m_selectedMediaId) ? m_selectedMediaId : (ids.isEmpty() ? -1 : ids.first()), m_selectionAnchorMediaId);
}

void AppController::clearSelection() { applySelection({}, -1, -1); }
void AppController::addTagsToSelectedMedia(const QVariantList &tags) { handleBatchActionResult(m_mediaActionsController->addTagsToMedia(selectedMediaIdVector(), tags, m_databaseReady, hasActiveBackgroundWork())); }
void AppController::removeTagsFromSelectedMedia(const QVariantList &tags) { handleBatchActionResult(m_mediaActionsController->removeTagsFromMedia(selectedMediaIdVector(), tags, m_databaseReady, hasActiveBackgroundWork())); }
void AppController::setSelectedMediaReviewStatus(const QString &reviewStatus) { handleBatchActionResult(m_mediaActionsController->setReviewStatusForMedia(selectedMediaIdVector(), reviewStatus, m_databaseReady, hasActiveBackgroundWork())); }
void AppController::setSelectedMediaRating(int rating) { handleBatchActionResult(m_mediaActionsController->setRatingForMedia(selectedMediaIdVector(), rating, m_databaseReady, hasActiveBackgroundWork())); }

void AppController::applySelection(const QVector<int> &mediaIds, int primaryMediaId, int anchorMediaId)
{
    const int oldIndex = m_selectedIndex; const int oldMediaId = m_selectedMediaId; const QVector<int> oldIds = m_selectedMediaIds;
    m_selectedMediaIds = visibleMediaIds(mediaIds);
    if (primaryMediaId <= 0 || !containsMediaId(m_selectedMediaIds, primaryMediaId)) primaryMediaId = m_selectedMediaIds.isEmpty() ? -1 : m_selectedMediaIds.first();
    m_selectedMediaId = primaryMediaId; m_selectedIndex = m_mediaLibraryModel && primaryMediaId > 0 ? m_mediaLibraryModel->indexOfId(primaryMediaId) : -1;
    m_selectionAnchorMediaId = anchorMediaId > 0 && m_mediaLibraryModel && m_mediaLibraryModel->indexOfId(anchorMediaId) >= 0 ? anchorMediaId : primaryMediaId;
    if (m_mediaLibraryModel) m_mediaLibraryModel->setSelectedMediaIds(m_selectedMediaIds);
    if (oldIndex != m_selectedIndex) emit selectedIndexChanged();
    if (oldMediaId != m_selectedMediaId) { emit selectedMediaChanged(); refreshSelectedSnapshots(); if (m_selectedMediaId > 0) maybeStartAutoMetadataExtraction(); }
    if (oldIds != m_selectedMediaIds) emit selectionStateChanged();
}

QVector<int> AppController::visibleMediaIds(const QVector<int> &mediaIds) const
{
    QVector<int> result; for (const int mediaId : mediaIds) if (mediaId > 0 && m_mediaLibraryModel && m_mediaLibraryModel->indexOfId(mediaId) >= 0) appendMediaIdOnce(&result, mediaId);
    return result;
}

QVector<int> AppController::selectedMediaIdVector() const { return m_selectedMediaIds; }

void AppController::handleBatchActionResult(const MediaActionResult &result)
{
    if (result.selectedMediaChanged) emit selectedMediaChanged();
    if (result.libraryReloadRequested) { refreshAvailableTags(); requestLibraryReload(0); }
    setDetailStatus(result.status);
}

#include "app/AppController.h"

#include "application/BatchRenameMediaUseCase.h"
#include "media/MediaLibraryModel.h"
#include "media/RenameService.h"

namespace {
BatchRenameRule ruleFromMap(const QVariantMap &map)
{
    BatchRenameRule rule;
    rule.prefix = map.value(QStringLiteral("prefix")).toString();
    rule.suffix = map.value(QStringLiteral("suffix")).toString();
    rule.numberingEnabled = map.value(QStringLiteral("numberingEnabled")).toBool();
    rule.numberStart = map.value(QStringLiteral("numberStart"), 1).toInt();
    rule.numberPadding = map.value(QStringLiteral("numberPadding"), 2).toInt();
    return rule;
}

QVariantMap previewItemToMap(const BatchRenamePreviewItem &item)
{
    return {
        {QStringLiteral("mediaId"), item.mediaId},
        {QStringLiteral("currentName"), item.currentName},
        {QStringLiteral("currentPath"), item.currentPath},
        {QStringLiteral("newBaseName"), item.newBaseName},
        {QStringLiteral("newName"), item.newName},
        {QStringLiteral("newPath"), item.newPath},
        {QStringLiteral("status"), item.status},
        {QStringLiteral("runnable"), item.runnable},
        {QStringLiteral("succeeded"), item.succeeded},
    };
}

QVariantList previewItemsToList(const QVector<BatchRenamePreviewItem> &items)
{
    QVariantList list;
    list.reserve(items.size());
    for (const BatchRenamePreviewItem &item : items) {
        list.append(previewItemToMap(item));
    }
    return list;
}

QVariantMap previewResultToMap(const BatchRenamePreviewResult &result)
{
    return {
        {QStringLiteral("status"), result.status},
        {QStringLiteral("items"), previewItemsToList(result.items)},
        {QStringLiteral("runnableCount"), result.runnableCount},
        {QStringLiteral("invalidCount"), result.invalidCount},
        {QStringLiteral("succeeded"), result.succeeded},
    };
}

QVariantMap executionResultToMap(const BatchRenameExecutionResult &result)
{
    return {
        {QStringLiteral("status"), result.status},
        {QStringLiteral("items"), previewItemsToList(result.items)},
        {QStringLiteral("successCount"), result.successCount},
        {QStringLiteral("failureCount"), result.failureCount},
        {QStringLiteral("skippedCount"), result.skippedCount},
        {QStringLiteral("succeeded"), result.succeeded},
    };
}

QVariantMap statusMap(const QString &status)
{
    return {
        {QStringLiteral("status"), status},
        {QStringLiteral("items"), QVariantList {}},
        {QStringLiteral("runnableCount"), 0},
        {QStringLiteral("invalidCount"), 0},
        {QStringLiteral("successCount"), 0},
        {QStringLiteral("failureCount"), 0},
        {QStringLiteral("skippedCount"), 0},
        {QStringLiteral("succeeded"), false},
    };
}
}

QVector<MediaLibraryItem> AppController::selectedMediaItemsInVisibleOrder() const
{
    QVector<MediaLibraryItem> items;
    if (!m_mediaLibraryModel || m_selectedMediaIds.size() < 2) {
        return items;
    }
    items.reserve(m_selectedMediaIds.size());
    for (int row = 0; row < m_mediaLibraryModel->rowCount(); ++row) {
        const int mediaId = m_mediaLibraryModel->mediaIdAt(row);
        if (m_selectedMediaIds.contains(mediaId)) {
            items.append(m_mediaLibraryModel->itemAt(row));
        }
    }
    return items;
}

QVariantMap AppController::previewSelectedMediaBatchRename(const QVariantMap &rule) const
{
    const QVector<MediaLibraryItem> items = selectedMediaItemsInVisibleOrder();
    if (items.size() < 2) {
        return statusMap(QStringLiteral("Select at least two visible media items before batch rename."));
    }
    RenameService renamer;
    BatchRenameMediaUseCase useCase(&renamer, m_mediaRepository);
    return previewResultToMap(useCase.preview(items, ruleFromMap(rule)));
}

QVariantMap AppController::renameSelectedMediaBatch(const QVariantMap &rule)
{
    const QVector<MediaLibraryItem> items = selectedMediaItemsInVisibleOrder();
    if (items.size() < 2) {
        const QVariantMap result = statusMap(QStringLiteral("Select at least two visible media items before batch rename."));
        setFileActionStatus(result.value(QStringLiteral("status")).toString());
        return result;
    }
    if (!m_databaseReady || !m_mediaRepository) {
        const QVariantMap result = statusMap(QStringLiteral("Database is not ready for batch rename."));
        setFileActionStatus(result.value(QStringLiteral("status")).toString());
        return result;
    }
    if (hasActiveBackgroundWork()) {
        const QVariantMap result = statusMap(QStringLiteral("Wait for active work to finish before batch rename."));
        setFileActionStatus(result.value(QStringLiteral("status")).toString());
        return result;
    }

    RenameService renamer;
    BatchRenameMediaUseCase useCase(&renamer, m_mediaRepository);
    const BatchRenameExecutionResult execution = useCase.execute(items, ruleFromMap(rule));
    setFileActionStatus(execution.status);
    if (execution.successCount > 0) {
        requestLibraryReload(0);
    }
    return executionResultToMap(execution);
}

#include "BatchRenameMediaUseCase.h"

#include "application/RenameMediaUseCase.h"
#include "ports/IFileRenamer.h"
#include "ports/IMediaRepository.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace {
QString originalBaseName(const MediaLibraryItem &item)
{
    const QString name = item.fileName.isEmpty() ? QFileInfo(item.filePath).fileName() : item.fileName;
    return QFileInfo(name).completeBaseName();
}

QString normalizedExtension(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix.isEmpty() ? QString() : QStringLiteral(".%1").arg(suffix);
}

QString normalizedPathKey(const QString &path)
{
    return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toLower();
}

BatchRenameRule normalizedRule(BatchRenameRule rule)
{
    rule.numberStart = std::max(1, rule.numberStart);
    rule.numberPadding = std::clamp(rule.numberPadding, 1, 9);
    return rule;
}

QString candidateBaseName(const MediaLibraryItem &item, const BatchRenameRule &rule, int index)
{
    QString baseName = rule.prefix + originalBaseName(item) + rule.suffix;
    if (rule.numberingEnabled) {
        baseName += QStringLiteral("-%1").arg(rule.numberStart + index, rule.numberPadding, 10, QLatin1Char('0'));
    }
    return baseName;
}

void updatePreviewCounts(BatchRenamePreviewResult *result)
{
    result->runnableCount = 0;
    result->invalidCount = 0;
    for (const BatchRenamePreviewItem &item : result->items) {
        if (item.runnable) {
            ++result->runnableCount;
        } else {
            ++result->invalidCount;
        }
    }
    result->status = result->runnableCount > 0
        ? QStringLiteral("%1 item(s) ready to rename").arg(result->runnableCount)
        : QStringLiteral("No item can be renamed");
    result->succeeded = !result->items.isEmpty();
}
}

BatchRenameMediaUseCase::BatchRenameMediaUseCase(IFileRenamer *renamer, IMediaRepository *repository)
    : m_renamer(renamer)
    , m_repository(repository)
{
}

BatchRenamePreviewResult BatchRenameMediaUseCase::preview(
    const QVector<MediaLibraryItem> &items,
    const BatchRenameRule &rawRule) const
{
    BatchRenamePreviewResult result;
    if (!m_renamer) {
        result.status = QStringLiteral("File renamer is unavailable.");
        return result;
    }
    if (items.isEmpty()) {
        result.status = QStringLiteral("Select media items before batch rename.");
        return result;
    }

    const BatchRenameRule rule = normalizedRule(rawRule);
    QHash<QString, QVector<int>> rowsByTarget;
    result.items.reserve(items.size());
    for (int index = 0; index < items.size(); ++index) {
        const MediaLibraryItem &item = items.at(index);
        BatchRenamePreviewItem previewItem;
        previewItem.mediaId = item.id;
        previewItem.currentName = item.fileName;
        previewItem.currentPath = item.filePath;
        previewItem.newBaseName = candidateBaseName(item, rule, index);

        if (item.id <= 0 || item.filePath.trimmed().isEmpty()) {
            previewItem.status = QStringLiteral("Invalid media item.");
            result.items.append(previewItem);
            continue;
        }

        const FileRenameResult plan = m_renamer->planRename(item.filePath, previewItem.newBaseName);
        if (!plan.succeeded) {
            previewItem.status = plan.errorMessage;
            result.items.append(previewItem);
            continue;
        }

        const QString originalExtension = normalizedExtension(item.filePath);
        if (plan.file.fileExtension.compare(originalExtension, Qt::CaseInsensitive) != 0) {
            previewItem.status = QStringLiteral("Extension would change.");
            result.items.append(previewItem);
            continue;
        }

        previewItem.newName = plan.file.fileName;
        previewItem.newPath = plan.file.filePath;
        if (normalizedPathKey(previewItem.currentPath) == normalizedPathKey(previewItem.newPath)) {
            previewItem.status = QStringLiteral("Unchanged.");
            result.items.append(previewItem);
            continue;
        }

        previewItem.status = QStringLiteral("Ready.");
        previewItem.runnable = true;
        rowsByTarget[normalizedPathKey(previewItem.newPath)].append(result.items.size());
        result.items.append(previewItem);
    }

    for (const QVector<int> &rows : rowsByTarget) {
        if (rows.size() < 2) {
            continue;
        }
        for (const int row : rows) {
            result.items[row].runnable = false;
            result.items[row].status = QStringLiteral("Target duplicates another selected item.");
        }
    }

    updatePreviewCounts(&result);
    return result;
}

BatchRenameExecutionResult BatchRenameMediaUseCase::execute(
    const QVector<MediaLibraryItem> &items,
    const BatchRenameRule &rule) const
{
    BatchRenameExecutionResult result;
    if (!m_repository) {
        result.status = QStringLiteral("Repository is unavailable.");
        return result;
    }

    const BatchRenamePreviewResult previewResult = preview(items, rule);
    result.items = previewResult.items;
    if (!previewResult.succeeded) {
        result.status = previewResult.status;
        return result;
    }

    RenameMediaUseCase renameUseCase(m_renamer, m_repository);
    for (BatchRenamePreviewItem &item : result.items) {
        if (!item.runnable) {
            ++result.skippedCount;
            continue;
        }

        const OperationResult<ScannedMediaFile> renamed = renameUseCase.execute(
            item.mediaId,
            item.currentPath,
            item.newBaseName,
            QFileInfo(item.currentPath).completeBaseName());
        if (!renamed.succeeded) {
            item.status = QStringLiteral("Rename failed: %1").arg(renamed.errorMessage);
            item.succeeded = false;
            ++result.failureCount;
            continue;
        }

        item.newName = renamed.value.fileName;
        item.newPath = renamed.value.filePath;
        item.status = QStringLiteral("Renamed.");
        item.succeeded = true;
        ++result.successCount;
    }

    result.status = QStringLiteral("Renamed %1 item(s), skipped %2, failed %3")
        .arg(result.successCount)
        .arg(result.skippedCount)
        .arg(result.failureCount);
    result.succeeded = result.failureCount == 0 && result.successCount > 0;
    return result;
}

#include "MediaLibraryModel.h"

#include <utility>

MediaLibraryModel::MediaLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MediaLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_items.size());
}

QVariant MediaLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    return valueForRole(m_items.at(index.row()), role);
}

QHash<int, QByteArray> MediaLibraryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {FileNameRole, "fileName"},
        {FilePathRole, "filePath"},
        {FileSizeRole, "fileSize"},
        {FileSizeBytesRole, "fileSizeBytes"},
        {DurationRole, "duration"},
        {DurationMsRole, "durationMs"},
        {ResolutionRole, "resolution"},
        {CodecRole, "codec"},
        {DescriptionRole, "description"},
        {TagsRole, "tags"},
        {ReviewStatusRole, "reviewStatus"},
        {RatingRole, "rating"},
        {ModifiedAtRole, "modifiedAt"},
        {ThumbnailPathRole, "thumbnailPath"},
    };
}

QVariantMap MediaLibraryModel::get(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    return toMap(m_items.at(row));
}

int MediaLibraryModel::mediaIdAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return -1;
    }

    return m_items.at(row).id;
}

int MediaLibraryModel::indexOfId(int mediaId) const
{
    if (mediaId <= 0) {
        return -1;
    }

    for (int index = 0; index < m_items.size(); ++index) {
        if (m_items.at(index).id == mediaId) {
            return index;
        }
    }

    return -1;
}

void MediaLibraryModel::setItems(QVector<MediaLibraryItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

QVariant MediaLibraryModel::valueForRole(const MediaLibraryItem &item, int role) const
{
    switch (role) {
    case IdRole:
        return item.id;
    case FileNameRole:
        return item.fileName;
    case FilePathRole:
        return item.filePath;
    case FileSizeRole:
        return item.fileSize;
    case FileSizeBytesRole:
        return item.fileSizeBytes;
    case DurationRole:
        return item.duration;
    case DurationMsRole:
        return item.durationMs;
    case ResolutionRole:
        return item.resolution;
    case CodecRole:
        return item.codec;
    case DescriptionRole:
        return item.description;
    case TagsRole:
        return item.tags;
    case ReviewStatusRole:
        return item.reviewStatus;
    case RatingRole:
        return item.rating;
    case ModifiedAtRole:
        return item.modifiedAt;
    case ThumbnailPathRole:
        return item.thumbnailPath;
    default:
        return {};
    }
}

QVariantMap MediaLibraryModel::toMap(const MediaLibraryItem &item) const
{
    return {
        {"id", item.id},
        {"fileName", item.fileName},
        {"filePath", item.filePath},
        {"fileSize", item.fileSize},
        {"fileSizeBytes", item.fileSizeBytes},
        {"duration", item.duration},
        {"durationMs", item.durationMs},
        {"resolution", item.resolution},
        {"codec", item.codec},
        {"description", item.description},
        {"tags", item.tags},
        {"reviewStatus", item.reviewStatus},
        {"rating", item.rating},
        {"modifiedAt", item.modifiedAt},
        {"thumbnailPath", item.thumbnailPath},
    };
}

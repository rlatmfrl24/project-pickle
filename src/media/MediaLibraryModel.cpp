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
        {BitrateRole, "bitrate"},
        {BitrateBpsRole, "bitrateBps"},
        {FrameRateRole, "frameRate"},
        {FrameRateValueRole, "frameRateValue"},
        {DescriptionRole, "description"},
        {TagsRole, "tags"},
        {ReviewStatusRole, "reviewStatus"},
        {RatingRole, "rating"},
        {ModifiedAtRole, "modifiedAt"},
        {ThumbnailPathRole, "thumbnailPath"},
        {IsFavoriteRole, "isFavorite"},
        {IsDeleteCandidateRole, "isDeleteCandidate"},
        {LastPositionMsRole, "lastPositionMs"},
        {LastPlayedAtRole, "lastPlayedAt"},
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

    return m_rowById.value(mediaId, -1);
}

MediaLibraryItem MediaLibraryModel::itemAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    return m_items.at(row);
}

void MediaLibraryModel::setItems(QVector<MediaLibraryItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    rebuildIndex();
    endResetModel();
}

bool MediaLibraryModel::replaceItem(int mediaId, const MediaLibraryItem &item)
{
    const int row = indexOfId(mediaId);
    if (row < 0) {
        return false;
    }

    m_items[row] = item;
    if (item.id != mediaId) {
        rebuildIndex();
    }

    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {
        IdRole,
        FileNameRole,
        FilePathRole,
        FileSizeRole,
        FileSizeBytesRole,
        DurationRole,
        DurationMsRole,
        ResolutionRole,
        CodecRole,
        BitrateRole,
        BitrateBpsRole,
        FrameRateRole,
        FrameRateValueRole,
        DescriptionRole,
        TagsRole,
        ReviewStatusRole,
        RatingRole,
        ModifiedAtRole,
        ThumbnailPathRole,
        IsFavoriteRole,
        IsDeleteCandidateRole,
        LastPositionMsRole,
        LastPlayedAtRole,
    });
    return true;
}

bool MediaLibraryModel::setFavorite(int mediaId, bool enabled)
{
    const int row = indexOfId(mediaId);
    if (row < 0 || m_items[row].isFavorite == enabled) {
        return row >= 0;
    }

    m_items[row].isFavorite = enabled;
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {IsFavoriteRole});
    return true;
}

bool MediaLibraryModel::setDeleteCandidate(int mediaId, bool enabled)
{
    const int row = indexOfId(mediaId);
    if (row < 0 || m_items[row].isDeleteCandidate == enabled) {
        return row >= 0;
    }

    m_items[row].isDeleteCandidate = enabled;
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {IsDeleteCandidateRole});
    return true;
}

bool MediaLibraryModel::setPlaybackPosition(int mediaId, qint64 positionMs, const QString &playedAt)
{
    const int row = indexOfId(mediaId);
    if (row < 0) {
        return false;
    }

    m_items[row].lastPositionMs = positionMs;
    m_items[row].lastPlayedAt = playedAt;
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {LastPositionMsRole, LastPlayedAtRole});
    return true;
}

bool MediaLibraryModel::setThumbnailPath(int mediaId, const QString &thumbnailPath)
{
    const int row = indexOfId(mediaId);
    if (row < 0 || m_items[row].thumbnailPath == thumbnailPath) {
        return row >= 0;
    }

    m_items[row].thumbnailPath = thumbnailPath;
    const QModelIndex modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex, {ThumbnailPathRole});
    return true;
}

void MediaLibraryModel::rebuildIndex()
{
    m_rowById.clear();
    m_rowById.reserve(m_items.size());
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).id > 0) {
            m_rowById.insert(m_items.at(row).id, row);
        }
    }
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
    case BitrateRole:
        return item.bitrate;
    case BitrateBpsRole:
        return item.bitrateBps;
    case FrameRateRole:
        return item.frameRate;
    case FrameRateValueRole:
        return item.frameRateValue;
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
    case IsFavoriteRole:
        return item.isFavorite;
    case IsDeleteCandidateRole:
        return item.isDeleteCandidate;
    case LastPositionMsRole:
        return item.lastPositionMs;
    case LastPlayedAtRole:
        return item.lastPlayedAt;
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
        {"bitrate", item.bitrate},
        {"bitrateBps", item.bitrateBps},
        {"frameRate", item.frameRate},
        {"frameRateValue", item.frameRateValue},
        {"description", item.description},
        {"tags", item.tags},
        {"reviewStatus", item.reviewStatus},
        {"rating", item.rating},
        {"modifiedAt", item.modifiedAt},
        {"thumbnailPath", item.thumbnailPath},
        {"isFavorite", item.isFavorite},
        {"isDeleteCandidate", item.isDeleteCandidate},
        {"lastPositionMs", item.lastPositionMs},
        {"lastPlayedAt", item.lastPlayedAt},
    };
}

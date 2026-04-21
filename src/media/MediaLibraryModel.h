#pragma once

#include "media/MediaTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QVariantMap>

class MediaLibraryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
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
        IsSelectedRole
    };

    explicit MediaLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int mediaIdAt(int row) const;
    Q_INVOKABLE int indexOfId(int mediaId) const;
    Q_INVOKABLE bool isSelected(int row) const;

    MediaLibraryItem itemAt(int row) const;
    void setItems(QVector<MediaLibraryItem> items);
    void setSelectedMediaIds(const QVector<int> &mediaIds);
    bool replaceItem(int mediaId, const MediaLibraryItem &item);
    bool setFavorite(int mediaId, bool enabled);
    bool setDeleteCandidate(int mediaId, bool enabled);
    bool setPlaybackPosition(int mediaId, qint64 positionMs, const QString &playedAt);
    bool setThumbnailPath(int mediaId, const QString &thumbnailPath);

private:
    void rebuildIndex();
    QVariant valueForRole(const MediaLibraryItem &item, int role) const;
    QVariantMap toMap(const MediaLibraryItem &item) const;

    QVector<MediaLibraryItem> m_items;
    QHash<int, int> m_rowById;
    QSet<int> m_selectedIds;
};

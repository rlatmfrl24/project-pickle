#pragma once

#include "media/MediaTypes.h"

#include <QAbstractListModel>
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
        DescriptionRole,
        TagsRole,
        ReviewStatusRole,
        RatingRole,
        ModifiedAtRole,
        ThumbnailPathRole
    };

    explicit MediaLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int mediaIdAt(int row) const;
    Q_INVOKABLE int indexOfId(int mediaId) const;

    void setItems(QVector<MediaLibraryItem> items);

private:
    QVariant valueForRole(const MediaLibraryItem &item, int role) const;
    QVariantMap toMap(const MediaLibraryItem &item) const;

    QVector<MediaLibraryItem> m_items;
};

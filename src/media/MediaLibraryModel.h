#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <vector>

class MediaLibraryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        FileNameRole,
        FilePathRole,
        FileSizeRole,
        DurationRole,
        ResolutionRole,
        CodecRole,
        DescriptionRole,
        TagsRole,
        ReviewStatusRole,
        RatingRole,
        ModifiedAtRole
    };

    explicit MediaLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;

private:
    struct MediaItem {
        int id = 0;
        QString fileName;
        QString filePath;
        QString fileSize;
        QString duration;
        QString resolution;
        QString codec;
        QString description;
        QStringList tags;
        QString reviewStatus;
        int rating = 0;
        QString modifiedAt;
    };

    QVariant valueForRole(const MediaItem &item, int role) const;
    QVariantMap toMap(const MediaItem &item) const;
    void loadMockItems();

    std::vector<MediaItem> m_items;
};

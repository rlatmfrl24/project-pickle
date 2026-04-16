#pragma once

#include <QObject>
#include <QVariantMap>

class MediaLibraryModel;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedMedia READ selectedMedia NOTIFY selectedMediaChanged)

public:
    explicit AppController(MediaLibraryModel *mediaLibraryModel, QObject *parent = nullptr);

    int selectedIndex() const;
    void setSelectedIndex(int selectedIndex);

    QVariantMap selectedMedia() const;

    Q_INVOKABLE void selectIndex(int index);

signals:
    void selectedIndexChanged();
    void selectedMediaChanged();

private:
    MediaLibraryModel *m_mediaLibraryModel = nullptr;
    int m_selectedIndex = 0;
};

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class PlaybackController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasSource READ hasSource NOTIFY sourceChanged)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceFilePath READ sourceFilePath NOTIFY sourceChanged)

public:
    explicit PlaybackController(QObject *parent = nullptr);

    bool hasSource() const;
    QUrl sourceUrl() const;
    QString sourceFilePath() const;

public slots:
    void setMedia(const QVariantMap &media);
    void clearSource();

signals:
    void sourceChanged();

private:
    QString m_sourceFilePath;
    QUrl m_sourceUrl;
};

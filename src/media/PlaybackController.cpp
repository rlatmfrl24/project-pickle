#include "PlaybackController.h"

PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent)
{
}

bool PlaybackController::hasSource() const
{
    return !m_sourceUrl.isEmpty();
}

QUrl PlaybackController::sourceUrl() const
{
    return m_sourceUrl;
}

QString PlaybackController::sourceFilePath() const
{
    return m_sourceFilePath;
}

void PlaybackController::setMedia(const QVariantMap &media)
{
    const QString filePath = media.value(QStringLiteral("filePath")).toString();
    const QUrl sourceUrl = filePath.isEmpty() ? QUrl() : QUrl::fromLocalFile(filePath);

    if (m_sourceFilePath == filePath && m_sourceUrl == sourceUrl) {
        return;
    }

    m_sourceFilePath = filePath;
    m_sourceUrl = sourceUrl;
    emit sourceChanged();
}

void PlaybackController::clearSource()
{
    setMedia({});
}

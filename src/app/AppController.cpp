#include "AppController.h"

#include "media/MediaLibraryModel.h"

AppController::AppController(MediaLibraryModel *mediaLibraryModel, QObject *parent)
    : QObject(parent)
    , m_mediaLibraryModel(mediaLibraryModel)
{
    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) {
        m_selectedIndex = -1;
    }
}

int AppController::selectedIndex() const
{
    return m_selectedIndex;
}

void AppController::setSelectedIndex(int selectedIndex)
{
    if (!m_mediaLibraryModel || m_mediaLibraryModel->rowCount() == 0) {
        selectedIndex = -1;
    } else if (selectedIndex < 0 || selectedIndex >= m_mediaLibraryModel->rowCount()) {
        return;
    }

    if (m_selectedIndex == selectedIndex) {
        return;
    }

    m_selectedIndex = selectedIndex;
    emit selectedIndexChanged();
    emit selectedMediaChanged();
}

QVariantMap AppController::selectedMedia() const
{
    if (!m_mediaLibraryModel || m_selectedIndex < 0) {
        return {};
    }

    return m_mediaLibraryModel->get(m_selectedIndex);
}

void AppController::selectIndex(int index)
{
    setSelectedIndex(index);
}

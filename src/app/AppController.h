#pragma once

#include "media/MediaTypes.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class MediaRepository;
class MediaLibraryModel;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedMedia READ selectedMedia NOTIFY selectedMediaChanged)
    Q_PROPERTY(bool databaseReady READ databaseReady NOTIFY databaseStateChanged)
    Q_PROPERTY(QString databaseStatus READ databaseStatus NOTIFY databaseStateChanged)
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY databaseStateChanged)
    Q_PROPERTY(bool scanInProgress READ scanInProgress NOTIFY scanStateChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStateChanged)
    Q_PROPERTY(QString currentScanRoot READ currentScanRoot NOTIFY scanStateChanged)
    Q_PROPERTY(QString librarySearchText READ librarySearchText WRITE setLibrarySearchText NOTIFY libraryStateChanged)
    Q_PROPERTY(QString librarySortKey READ librarySortKey WRITE setLibrarySortKey NOTIFY libraryStateChanged)
    Q_PROPERTY(bool librarySortAscending READ librarySortAscending WRITE setLibrarySortAscending NOTIFY libraryStateChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails WRITE setShowThumbnails NOTIFY libraryStateChanged)
    Q_PROPERTY(QString libraryStatus READ libraryStatus NOTIFY libraryStateChanged)

public:
    explicit AppController(
        MediaLibraryModel *mediaLibraryModel,
        MediaRepository *mediaRepository,
        QObject *parent = nullptr);
    ~AppController() override;

    int selectedIndex() const;
    void setSelectedIndex(int selectedIndex);

    QVariantMap selectedMedia() const;

    bool databaseReady() const;
    QString databaseStatus() const;
    QString databasePath() const;
    void setDatabaseState(bool ready, const QString &status, const QString &path);

    bool scanInProgress() const;
    QString scanStatus() const;
    QString currentScanRoot() const;

    QString librarySearchText() const;
    void setLibrarySearchText(const QString &searchText);
    QString librarySortKey() const;
    void setLibrarySortKey(const QString &sortKey);
    bool librarySortAscending() const;
    void setLibrarySortAscending(bool ascending);
    bool showThumbnails() const;
    void setShowThumbnails(bool showThumbnails);
    QString libraryStatus() const;

    Q_INVOKABLE void selectIndex(int index);
    Q_INVOKABLE void startDirectoryScan(const QUrl &folderUrl);
    Q_INVOKABLE void rescanCurrentRoot();

signals:
    void selectedIndexChanged();
    void selectedMediaChanged();
    void databaseStateChanged();
    void scanStateChanged();
    void libraryStateChanged();

private:
    void startDirectoryScanPath(const QString &rootPath);
    void handleScanFinished();
    bool reloadLibraryFromRepository();
    void syncSelectionAfterLibraryChange();
    void setScanState(bool inProgress, const QString &status, const QString &currentScanRoot);
    void setLibraryStatus(const QString &status);
    MediaLibraryQuery libraryQuery() const;

    MediaLibraryModel *m_mediaLibraryModel = nullptr;
    MediaRepository *m_mediaRepository = nullptr;
    QFutureWatcher<DirectoryScanResult> m_scanWatcher;
    int m_selectedIndex = 0;
    int m_selectedMediaId = -1;
    bool m_databaseReady = false;
    QString m_databaseStatus = QStringLiteral("DB not initialized");
    QString m_databasePath;
    bool m_scanInProgress = false;
    QString m_scanStatus = QStringLiteral("No scan yet");
    QString m_currentScanRoot;
    QString m_librarySearchText;
    QString m_librarySortKey = QStringLiteral("name");
    bool m_librarySortAscending = true;
    bool m_showThumbnails = true;
    QString m_libraryStatus = QStringLiteral("Library not loaded");
};

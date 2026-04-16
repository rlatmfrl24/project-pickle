import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import "components" as Components
import "pages" as Pages

ApplicationWindow {
    id: window

    required property var mediaLibraryModel
    required property var appController
    required property var playbackController

    minimumWidth: 960
    minimumHeight: 600
    width: 1180
    height: 720
    visible: true
    title: qsTr("Pickle")
    color: "#111318"

    FolderDialog {
        id: folderDialog

        title: qsTr("Select media folder")
        onAccepted: window.appController.startDirectoryScan(selectedFolder)
    }

    header: ToolBar {
        background: Rectangle {
            color: "#151922"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 14

            Label {
                text: qsTr("Pickle")
                color: "#f3f6fb"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Label {
                text: qsTr("Library review workspace")
                color: "#8e97a8"
                font.pixelSize: 13
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Label {
                text: window.appController.databaseStatus
                color: window.appController.databaseReady ? "#8fd19e" : "#ffb4a8"
                font.pixelSize: 12
                elide: Text.ElideMiddle
                Layout.maximumWidth: 220
            }

            Label {
                text: window.appController.scanStatus
                visible: text.length > 0
                color: window.appController.scanInProgress ? "#9fc9ff" : "#8e97a8"
                font.pixelSize: 12
                elide: Text.ElideMiddle
                Layout.maximumWidth: 260
            }

            Button {
                text: window.appController.scanInProgress ? qsTr("Scanning") : qsTr("Rescan")
                enabled: window.appController.databaseReady
                    && !window.appController.scanInProgress
                    && window.appController.currentScanRoot.length > 0
                onClicked: window.appController.rescanCurrentRoot()
            }

            Button {
                text: qsTr("Open Folder")
                enabled: window.appController.databaseReady && !window.appController.scanInProgress
                onClicked: folderDialog.open()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#111318"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 560
                spacing: 14

                Pages.PlayerPage {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    media: window.appController.selectedMedia
                    playbackController: window.playbackController
                }

                Components.PlaybackBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    media: window.appController.selectedMedia
                    playbackController: window.playbackController
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 372
                Layout.minimumWidth: 320
                Layout.maximumWidth: 430
                Layout.fillHeight: true
                spacing: 14

                Components.LibraryPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 430
                    model: window.mediaLibraryModel
                    currentIndex: window.appController.selectedIndex
                    searchText: window.appController.librarySearchText
                    sortKey: window.appController.librarySortKey
                    sortAscending: window.appController.librarySortAscending
                    showThumbnails: window.appController.showThumbnails
                    libraryStatus: window.appController.libraryStatus
                    onSelected: function(index) {
                        window.appController.selectIndex(index)
                    }
                    onSearchRequested: function(searchText) {
                        window.appController.librarySearchText = searchText
                    }
                    onSortKeyRequested: function(sortKey) {
                        window.appController.librarySortKey = sortKey
                    }
                    onSortAscendingRequested: function(ascending) {
                        window.appController.librarySortAscending = ascending
                    }
                    onThumbnailVisibilityRequested: function(showThumbnails) {
                        window.appController.showThumbnails = showThumbnails
                    }
                }

                Components.InfoPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 240
                    media: window.appController.selectedMedia
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "components" as Components
import "pages" as Pages

ApplicationWindow {
    id: window

    required property var mediaLibraryModel
    required property var appController

    minimumWidth: 960
    minimumHeight: 600
    width: 1180
    height: 720
    visible: true
    title: qsTr("Pickle")
    color: "#111318"

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

            Button {
                text: qsTr("Rescan")
                enabled: false
            }

            Button {
                text: qsTr("Open Folder")
                enabled: false
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
                }

                Components.PlaybackBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    media: window.appController.selectedMedia
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
                    onSelected: function(index) {
                        window.appController.selectIndex(index)
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

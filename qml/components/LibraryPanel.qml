pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var model
    property int currentIndex: -1
    property string searchText: ""
    property string sortKey: "name"
    property bool sortAscending: true
    property bool showThumbnails: true
    property string libraryStatus: ""

    signal selected(int index)
    signal searchRequested(string searchText)
    signal sortKeyRequested(string sortKey)
    signal sortAscendingRequested(bool ascending)
    signal thumbnailVisibilityRequested(bool showThumbnails)

    function thumbnailSource(path) {
        if (!path || path.length === 0) {
            return ""
        }

        if (path.indexOf("file:") === 0 || path.indexOf("qrc:") === 0) {
            return path
        }

        return "file:///" + path.replace(/\\/g, "/")
    }

    radius: 8
    color: "#171b24"
    border.color: "#283040"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: qsTr("Library")
                color: "#f3f6fb"
                font.pixelSize: 17
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            CheckBox {
                text: qsTr("Thumbs")
                checked: root.showThumbnails
                onToggled: root.thumbnailVisibilityRequested(checked)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                text: root.searchText
                placeholderText: qsTr("Search")
                Layout.fillWidth: true
                onTextEdited: root.searchRequested(text)
            }

            ComboBox {
                currentIndex: root.sortKey === "size" ? 1 : (root.sortKey === "modified" ? 2 : 0)
                model: [qsTr("Name"), qsTr("Size"), qsTr("Modified")]
                Layout.preferredWidth: 112
                onActivated: function(index) {
                    const keys = ["name", "size", "modified"]
                    root.sortKeyRequested(keys[index])
                }
            }

            Button {
                text: root.sortAscending ? qsTr("Asc") : qsTr("Desc")
                Layout.preferredWidth: 64
                onClicked: root.sortAscendingRequested(!root.sortAscending)
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.libraryStatus
            color: "#7f8898"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        ListView {
            id: listView

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.model
            currentIndex: root.currentIndex
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: row

                required property int index
                required property string fileName
                required property string fileSize
                required property string duration
                required property string reviewStatus
                required property int rating
                required property string thumbnailPath

                readonly property bool selected: root.currentIndex === row.index

                width: ListView.view.width
                height: 72
                radius: 6
                color: selected ? "#243348" : (mouseArea.containsMouse ? "#1d2431" : "#141922")
                border.color: selected ? "#5ab0ff" : "transparent"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        visible: root.showThumbnails
                        Layout.preferredWidth: root.showThumbnails ? 46 : 0
                        Layout.preferredHeight: 46
                        radius: 5
                        color: row.selected ? "#36506f" : "#252c38"
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: root.thumbnailSource(row.thumbnailPath)
                            visible: row.thumbnailPath.length > 0
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }

                        Label {
                            anchors.centerIn: parent
                            text: row.rating > 0 ? row.rating : "-"
                            color: "#d6e7ff"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            visible: row.thumbnailPath.length === 0
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: row.fileName
                            color: "#f3f6fb"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: row.fileSize + "  /  " + row.duration
                            color: "#8e97a8"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        text: row.reviewStatus
                        color: row.selected ? "#d6e7ff" : "#9fb0c5"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: mouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.selected(row.index)
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: listView.count === 0
                text: qsTr("No media")
                color: "#7f8898"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }
}

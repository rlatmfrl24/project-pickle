pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var model
    property int currentIndex: -1
    property string searchText: ""
    property string viewMode: "all"
    property string tagFilter: ""
    property var availableTags: []
    property string sortKey: "name"
    property bool sortAscending: true
    property bool showThumbnails: true
    property string libraryStatus: ""
    property int selectedCount: 0

    signal selected(int index)
    signal selectionRequested(int index, bool toggle, bool range)
    signal activated(int index)
    signal renameRequested(int index)
    signal selectAllRequested()
    signal clearSelectionRequested()
    signal searchRequested(string searchText)
    signal viewModeRequested(string viewMode)
    signal tagFilterRequested(string tagFilter)
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

    readonly property var viewKeys: ["all", "unreviewed", "favorites", "deleteCandidates", "recent"]
    readonly property var viewLabels: [qsTr("All"), qsTr("Unreviewed"), qsTr("Favorites"), qsTr("Delete"), qsTr("Recent")]

    function viewIndex(viewMode) {
        for (let i = 0; i < root.viewKeys.length; ++i) {
            if (root.viewKeys[i] === viewMode) {
                return i
            }
        }
        return 0
    }

    function tagOptions() {
        const options = [qsTr("All tags")]
        const tags = root.availableTags || []
        for (let i = 0; i < tags.length; ++i) {
            options.push(String(tags[i]))
        }
        return options
    }

    function tagIndex(tagFilter) {
        if (!tagFilter || tagFilter.length === 0) {
            return 0
        }
        const tags = root.availableTags || []
        for (let i = 0; i < tags.length; ++i) {
            if (String(tags[i]).toLocaleLowerCase() === tagFilter.toLocaleLowerCase()) {
                return i + 1
            }
        }
        return 0
    }

    radius: 8
    color: "#171b24"
    border.color: "#283040"
    border.width: 1
    focus: true

    function requestRenameCurrent() {
        if (root.currentIndex >= 0) {
            root.renameRequested(root.currentIndex)
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_F2) {
            root.requestRenameCurrent()
            event.accepted = true
        } else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier) {
            root.selectAllRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && root.selectedCount > 1) {
            root.clearSelectionRequested()
            event.accepted = true
        }
    }

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
                currentIndex: root.sortKey === "size" ? 1 : (root.sortKey === "modified" ? 2 : (root.sortKey === "lastPlayed" ? 3 : 0))
                model: [qsTr("Name"), qsTr("Size"), qsTr("Modified"), qsTr("Last played")]
                Layout.preferredWidth: 128
                onActivated: function(index) {
                    const keys = ["name", "size", "modified", "lastPlayed"]
                    root.sortKeyRequested(keys[index])
                }
            }

            Button {
                text: root.sortAscending ? qsTr("Asc") : qsTr("Desc")
                Layout.preferredWidth: 64
                onClicked: root.sortAscendingRequested(!root.sortAscending)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ComboBox {
                currentIndex: root.viewIndex(root.viewMode)
                model: root.viewLabels
                Layout.fillWidth: true
                onActivated: function(index) {
                    root.viewModeRequested(root.viewKeys[index])
                }
            }

            ComboBox {
                currentIndex: root.tagIndex(root.tagFilter)
                model: root.tagOptions()
                Layout.fillWidth: true
                onActivated: function(index) {
                    root.tagFilterRequested(index <= 0 ? "" : String(root.availableTags[index - 1]))
                }
            }

            Button {
                text: qsTr("Clear")
                enabled: root.tagFilter.length > 0
                Layout.preferredWidth: 58
                onClicked: root.tagFilterRequested("")
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.selectedCount > 1
                ? root.libraryStatus + qsTr("  /  ") + qsTr("%1 selected").arg(root.selectedCount)
                : root.libraryStatus
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
            focus: true
            reuseItems: true

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_F2) {
                    root.requestRenameCurrent()
                    event.accepted = true
                } else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier) {
                    root.selectAllRequested()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape && root.selectedCount > 1) {
                    root.clearSelectionRequested()
                    event.accepted = true
                }
            }

            delegate: Rectangle {
                id: row

                required property int index
                required property string fileName
                required property string fileSize
                required property string duration
                required property string reviewStatus
                required property int rating
                required property string thumbnailPath
                required property bool isFavorite
                required property bool isDeleteCandidate
                required property bool isSelected

                readonly property bool primarySelected: root.currentIndex === row.index

                width: ListView.view.width
                height: 72
                radius: 6
                color: primarySelected
                    ? "#243348"
                    : (row.isSelected ? "#1f2a3a" : (row.isDeleteCandidate ? "#251b22" : (mouseArea.containsMouse ? "#1d2431" : "#141922")))
                border.color: primarySelected ? "#5ab0ff" : (row.isSelected ? "#7fa7d8" : (row.isFavorite ? "#e0b04c" : "transparent"))
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
                        color: row.primarySelected ? "#36506f" : (row.isSelected ? "#31435b" : "#252c38")
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: root.showThumbnails ? root.thumbnailSource(row.thumbnailPath) : ""
                            sourceSize.width: 92
                            sourceSize.height: 92
                            visible: root.showThumbnails && row.thumbnailPath.length > 0
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
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
                        text: (row.isFavorite ? qsTr("Fav ") : "")
                            + (row.isDeleteCandidate ? qsTr("Delete ") : "")
                            + row.reviewStatus
                        color: row.isDeleteCandidate ? "#ffb4a8" : (row.primarySelected || row.isSelected ? "#d6e7ff" : "#9fb0c5")
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: mouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    onClicked: function(mouse) {
                        listView.forceActiveFocus()
                        root.selectionRequested(row.index, Boolean(mouse.modifiers & Qt.ControlModifier), Boolean(mouse.modifiers & Qt.ShiftModifier))
                    }
                    onDoubleClicked: {
                        listView.forceActiveFocus()
                        root.activated(row.index)
                    }
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

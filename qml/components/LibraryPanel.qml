pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var model
    property int currentIndex: -1

    signal selected(int index)

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
                checked: true
                enabled: false
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                placeholderText: qsTr("Search")
                enabled: false
                Layout.fillWidth: true
            }

            ComboBox {
                enabled: false
                model: [qsTr("Name"), qsTr("Size"), qsTr("Modified")]
                Layout.preferredWidth: 112
            }
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
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        radius: 5
                        color: row.selected ? "#36506f" : "#252c38"

                        Label {
                            anchors.centerIn: parent
                            text: row.rating > 0 ? row.rating : "-"
                            color: "#d6e7ff"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
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
        }
    }
}

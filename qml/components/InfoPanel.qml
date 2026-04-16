import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var media: ({})

    function displayValue(key, fallbackText) {
        if (!root.media) {
            return fallbackText
        }

        const value = root.media[key]
        return value === undefined || value === null || value === "" ? fallbackText : value
    }

    radius: 8
    color: "#171b24"
    border.color: "#283040"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Details")
            color: "#f3f6fb"
            font.pixelSize: 17
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: root.displayValue("fileName", qsTr("No selection"))
            color: "#dfe7f4"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 8
            columnSpacing: 12

            Label {
                text: qsTr("Size")
                color: "#7f8898"
                font.pixelSize: 12
            }

            Label {
                Layout.fillWidth: true
                text: root.displayValue("fileSize", qsTr("-"))
                color: "#c7d0de"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Duration")
                color: "#7f8898"
                font.pixelSize: 12
            }

            Label {
                Layout.fillWidth: true
                text: root.displayValue("duration", qsTr("-"))
                color: "#c7d0de"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Resolution")
                color: "#7f8898"
                font.pixelSize: 12
            }

            Label {
                Layout.fillWidth: true
                text: root.displayValue("resolution", qsTr("-"))
                color: "#c7d0de"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Codec")
                color: "#7f8898"
                font.pixelSize: 12
            }

            Label {
                Layout.fillWidth: true
                text: root.displayValue("codec", qsTr("-"))
                color: "#c7d0de"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Status")
                color: "#7f8898"
                font.pixelSize: 12
            }

            Label {
                Layout.fillWidth: true
                text: root.displayValue("reviewStatus", qsTr("-"))
                color: "#c7d0de"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#283040"
        }

        Label {
            Layout.fillWidth: true
            text: root.displayValue("description", qsTr("No description"))
            color: "#aeb7c7"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: root.media && root.media.tags ? root.media.tags : []

                delegate: Rectangle {
                    id: tag

                    required property string modelData

                    height: 26
                    width: tagLabel.implicitWidth + 18
                    radius: 13
                    color: "#243348"

                    Label {
                        id: tagLabel

                        anchors.centerIn: parent
                        text: tag.modelData
                        color: "#cfe2ff"
                        font.pixelSize: 12
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.displayValue("filePath", qsTr(""))
            color: "#6f7888"
            font.pixelSize: 11
            elide: Text.ElideMiddle
        }
    }
}

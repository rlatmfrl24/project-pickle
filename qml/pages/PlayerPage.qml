import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var media: ({})
    property var playbackController: null

    radius: 8
    color: "#07080b"
    border.color: "#242936"
    border.width: 1

    Rectangle {
        anchors.fill: parent
        anchors.margins: 18
        radius: 6
        color: "#0b0d12"
        border.color: "#1f2531"
        border.width: 1

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 520)
            spacing: 10

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Player Area")
                color: "#f3f6fb"
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: root.media && root.media.fileName ? root.media.fileName : qsTr("No selection")
                color: "#a8b0bf"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: root.playbackController && root.playbackController.hasSource
                    ? String(root.playbackController.sourceUrl)
                    : qsTr("No playable source")
                color: "#6f7888"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 18
                Layout.preferredWidth: 220
                Layout.preferredHeight: 3
                radius: 2
                color: "#303746"

                Rectangle {
                    width: parent.width * 0.36
                    height: parent.height
                    radius: parent.radius
                    color: "#5ab0ff"
                }
            }
        }
    }
}

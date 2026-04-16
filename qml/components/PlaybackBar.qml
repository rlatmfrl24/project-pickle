import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var media: ({})
    property var playbackController: null

    radius: 8
    color: "#171b24"
    border.color: "#283040"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 12

        Button {
            text: qsTr("Play")
            enabled: false
            Layout.preferredWidth: 74
        }

        Label {
            text: qsTr("00:00")
            color: "#aeb7c7"
            font.pixelSize: 12
        }

        Slider {
            from: 0
            to: 100
            value: 36
            enabled: false
            Layout.fillWidth: true
        }

        Label {
            text: root.media && root.media.duration ? root.media.duration : qsTr("--:--")
            color: "#aeb7c7"
            font.pixelSize: 12
        }

        Label {
            text: qsTr("Volume")
            color: "#7f8898"
            font.pixelSize: 12
        }

        Slider {
            from: 0
            to: 100
            value: 64
            enabled: false
            Layout.preferredWidth: 104
        }

        ComboBox {
            enabled: false
            model: ["1.0x", "1.25x", "1.5x", "2.0x"]
            Layout.preferredWidth: 86
        }
    }
}

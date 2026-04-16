import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root

    property var media: ({})
    property var playbackController: null
    property var mediaPlayer: null
    property var audioOutput: null
    property bool scrubbing: false
    property real scrubPosition: 0

    readonly property bool hasPlayer: mediaPlayer !== null
    readonly property bool canPlay: playbackController && playbackController.hasSource && hasPlayer
    readonly property bool isPlaying: canPlay && mediaPlayer.playbackState === MediaPlayer.PlayingState
    readonly property bool canSeek: canPlay && mediaPlayer.duration > 0 && mediaPlayer.seekable

    function durationMs() {
        if (root.hasPlayer && root.mediaPlayer.duration > 0) {
            return root.mediaPlayer.duration
        }

        return root.media && root.media.durationMs ? root.media.durationMs : 0
    }

    function positionMs() {
        if (root.scrubbing) {
            return root.scrubPosition
        }

        return root.hasPlayer ? root.mediaPlayer.position : 0
    }

    function formatTime(milliseconds) {
        if (!milliseconds || milliseconds < 0) {
            return qsTr("00:00")
        }

        const totalSeconds = Math.floor(milliseconds / 1000)
        const seconds = totalSeconds % 60
        const totalMinutes = Math.floor(totalSeconds / 60)
        const minutes = totalMinutes % 60
        const hours = Math.floor(totalMinutes / 60)
        const paddedSeconds = seconds < 10 ? "0" + seconds : String(seconds)
        const paddedMinutes = minutes < 10 ? "0" + minutes : String(minutes)

        if (hours > 0) {
            return hours + ":" + paddedMinutes + ":" + paddedSeconds
        }

        return minutes + ":" + paddedSeconds
    }

    function togglePlayback() {
        if (!root.canPlay) {
            return
        }

        if (root.mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            root.mediaPlayer.pause()
            return
        }

        if (root.mediaPlayer.mediaStatus === MediaPlayer.EndOfMedia) {
            root.mediaPlayer.position = 0
        }

        root.mediaPlayer.play()
    }

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
            text: root.isPlaying ? qsTr("Pause") : qsTr("Play")
            enabled: root.canPlay
            onClicked: root.togglePlayback()
            Layout.preferredWidth: 74
        }

        Label {
            text: root.formatTime(root.positionMs())
            color: "#aeb7c7"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 52
        }

        Slider {
            id: seekSlider

            from: 0
            to: Math.max(1, root.durationMs())
            value: Math.min(to, root.positionMs())
            enabled: root.canSeek
            Layout.fillWidth: true

            onPressedChanged: {
                if (pressed) {
                    root.scrubbing = true
                    root.scrubPosition = value
                } else {
                    if (root.canSeek) {
                        root.mediaPlayer.position = Math.round(value)
                    }
                    root.scrubbing = false
                }
            }

            onMoved: root.scrubPosition = value
        }

        Label {
            text: root.durationMs() > 0 ? root.formatTime(root.durationMs()) : qsTr("--:--")
            color: "#aeb7c7"
            font.pixelSize: 12
            Layout.preferredWidth: 52
        }

        Label {
            text: qsTr("Volume")
            color: "#7f8898"
            font.pixelSize: 12
        }

        Slider {
            from: 0
            to: 100
            value: root.audioOutput ? Math.round(root.audioOutput.volume * 100) : 64
            enabled: root.audioOutput !== null
            onMoved: {
                if (root.audioOutput) {
                    root.audioOutput.volume = value / 100
                }
            }
            Layout.preferredWidth: 104
        }

        ComboBox {
            enabled: false
            model: ["1.0x", "1.25x", "1.5x", "2.0x"]
            Layout.preferredWidth: 86
        }
    }
}

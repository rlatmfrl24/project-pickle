import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root

    property var media: ({})
    property var playbackController: null
    property alias mediaPlayer: player
    property alias audioOutput: output

    readonly property bool hasSource: playbackController && playbackController.hasSource
    readonly property bool hasPlaybackError: player.error !== MediaPlayer.NoError
        || player.mediaStatus === MediaPlayer.InvalidMedia

    function loadSelectedSource() {
        player.stop()
        player.source = root.hasSource ? root.playbackController.sourceUrl : ""
    }

    function overlayText() {
        if (!root.hasSource) {
            return qsTr("No selection")
        }

        if (root.hasPlaybackError) {
            return player.errorString.length > 0 ? player.errorString : qsTr("Unable to play media")
        }

        if (player.mediaStatus === MediaPlayer.LoadingMedia
                || player.mediaStatus === MediaPlayer.BufferingMedia
                || player.mediaStatus === MediaPlayer.StalledMedia) {
            return qsTr("Loading")
        }

        if (player.playbackState === MediaPlayer.PlayingState
                || player.playbackState === MediaPlayer.PausedState) {
            return ""
        }

        if (player.playbackState === MediaPlayer.StoppedState
                || player.mediaStatus === MediaPlayer.LoadedMedia
                || player.mediaStatus === MediaPlayer.BufferedMedia) {
            return qsTr("Ready to play")
        }

        return ""
    }

    radius: 8
    color: "#07080b"
    border.color: "#242936"
    border.width: 1

    Component.onCompleted: loadSelectedSource()
    onPlaybackControllerChanged: loadSelectedSource()

    Connections {
        target: root.playbackController
        ignoreUnknownSignals: true

        function onSourceChanged() {
            root.loadSelectedSource()
        }
    }

    AudioOutput {
        id: output

        volume: 0.64
    }

    MediaPlayer {
        id: player

        audioOutput: output
        videoOutput: videoOutput
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 18
        radius: 6
        color: "#0b0d12"
        border.color: "#1f2531"
        border.width: 1
        clip: true

        VideoOutput {
            id: videoOutput

            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            visible: root.hasSource && !root.hasPlaybackError
        }

        Rectangle {
            anchors.fill: parent
            visible: overlayColumn.visible
            color: "#0b0d12"
            opacity: 0.74
        }

        ColumnLayout {
            id: overlayColumn

            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 520)
            spacing: 10
            visible: root.overlayText().length > 0

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: root.media && root.media.fileName ? root.media.fileName : qsTr("Player Area")
                color: "#f3f6fb"
                font.pixelSize: 24
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: root.overlayText()
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
        }
    }
}

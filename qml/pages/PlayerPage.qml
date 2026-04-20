import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root

    property var media: ({})
    property var playbackController: null
    property var appController: null
    property alias mediaPlayer: player
    property alias audioOutput: output
    property real pendingRestorePosition: 0

    readonly property bool hasSource: playbackController && playbackController.hasSource
    readonly property bool isLoading: player.mediaStatus === MediaPlayer.LoadingMedia
        || player.mediaStatus === MediaPlayer.BufferingMedia
        || player.mediaStatus === MediaPlayer.StalledMedia
    readonly property bool hasPlaybackError: player.error !== MediaPlayer.NoError
        || player.mediaStatus === MediaPlayer.InvalidMedia
    readonly property bool canControlPlayback: root.hasSource && !root.hasPlaybackError
    readonly property bool canSeek: root.canControlPlayback && player.duration > 0 && player.seekable

    function loadSelectedSource() {
        player.stop()
        root.pendingRestorePosition = root.restorePositionForMedia()
        player.source = root.hasSource ? root.playbackController.sourceUrl : ""
        root.restorePendingPosition()
    }

    function syncSelectedSource() {
        const selectedSource = root.hasSource ? root.playbackController.sourceUrl : ""
        if (String(player.source) === String(selectedSource)) {
            return
        }

        player.stop()
        root.pendingRestorePosition = root.restorePositionForMedia()
        player.source = selectedSource
        root.restorePendingPosition()
    }

    function releaseLoadedSource() {
        root.persistPlaybackPosition()
        player.stop()
        root.pendingRestorePosition = 0
        player.source = ""
    }

    function playLoadedSource() {
        root.syncSelectedSource()
        if (!root.canControlPlayback) {
            return
        }

        root.restorePendingPosition()
        if (player.mediaStatus === MediaPlayer.EndOfMedia) {
            player.position = 0
        }

        player.play()
    }

    function togglePlayback() {
        root.syncSelectedSource()
        if (!root.canControlPlayback) {
            return
        }

        if (player.playbackState === MediaPlayer.PlayingState) {
            player.pause()
            root.persistPlaybackPosition()
            return
        }

        root.playLoadedSource()
    }

    function stopPlayback() {
        if (!root.hasSource) {
            return
        }

        root.persistPlaybackPosition()
        player.stop()
        if (player.duration > 0 && player.seekable) {
            player.position = 0
        }
    }

    function seekBy(deltaMs) {
        if (!root.canSeek) {
            return
        }

        const targetPosition = Math.max(0, Math.min(player.duration, player.position + deltaMs))
        player.position = Math.round(targetPosition)
    }

    function restorePositionForMedia() {
        const position = root.media && root.media.lastPositionMs ? Number(root.media.lastPositionMs) : 0
        if (!position || position < 5000) {
            return 0
        }

        const knownDuration = root.media && root.media.durationMs ? Number(root.media.durationMs) : 0
        if (knownDuration > 0 && position >= knownDuration - 5000) {
            return 0
        }

        return position
    }

    function restorePendingPosition() {
        if (!root.hasSource || root.pendingRestorePosition <= 0 || !player.seekable) {
            return
        }

        const duration = player.duration > 0 ? player.duration : (root.media && root.media.durationMs ? Number(root.media.durationMs) : 0)
        if (duration > 0 && root.pendingRestorePosition >= duration - 5000) {
            root.pendingRestorePosition = 0
            return
        }

        player.position = Math.round(root.pendingRestorePosition)
        root.pendingRestorePosition = 0
    }

    function persistPlaybackPosition() {
        if (!root.appController || !root.hasSource || player.mediaStatus === MediaPlayer.NoMedia) {
            return
        }

        const duration = player.duration > 0 ? player.duration : (root.media && root.media.durationMs ? Number(root.media.durationMs) : 0)
        let position = player.position > 0 ? player.position : 0
        if (duration > 0 && position >= duration - 5000) {
            position = 0
        }

        root.appController.saveSelectedPlaybackPosition(Math.round(position))
    }

    function overlayText() {
        if (!root.hasSource) {
            return qsTr("No selection")
        }

        if (root.hasPlaybackError) {
            return qsTr("Unable to play media")
        }

        if (root.isLoading) {
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

    function overlayDetailText() {
        if (root.hasPlaybackError && player.errorString.length > 0) {
            return player.errorString
        }

        return ""
    }

    radius: 8
    color: "#07080b"
    border.color: "#242936"
    border.width: 1
    focus: true

    Keys.onSpacePressed: function(event) {
        if (root.canControlPlayback) {
            root.togglePlayback()
            event.accepted = true
        }
    }

    Keys.onLeftPressed: function(event) {
        if (root.canSeek) {
            root.seekBy(-5000)
            event.accepted = true
        }
    }

    Keys.onRightPressed: function(event) {
        if (root.canSeek) {
            root.seekBy(5000)
            event.accepted = true
        }
    }

    Component.onCompleted: loadSelectedSource()
    onPlaybackControllerChanged: loadSelectedSource()

    Connections {
        target: root.playbackController
        ignoreUnknownSignals: true

        function onSourceChanged() {
            root.loadSelectedSource()
        }
    }

    Connections {
        target: root.appController
        ignoreUnknownSignals: true

        function onSettingsStateChanged() {
            if (root.appController) {
                output.volume = root.appController.playerVolume
                output.muted = root.appController.playerMuted
            }
        }
    }

    AudioOutput {
        id: output

        volume: root.appController ? root.appController.playerVolume : 0.64
        muted: root.appController ? root.appController.playerMuted : false
    }

    MediaPlayer {
        id: player

        audioOutput: output
        videoOutput: videoOutput

        onMediaStatusChanged: root.restorePendingPosition()
        onDurationChanged: root.restorePendingPosition()
        onSeekableChanged: root.restorePendingPosition()
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

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            cursorShape: root.hasSource ? Qt.PointingHandCursor : Qt.ArrowCursor

            onClicked: {
                root.forceActiveFocus()
                root.togglePlayback()
            }
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

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: root.isLoading
                visible: root.isLoading
            }

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
                visible: text.length > 0
                text: root.overlayDetailText()
                color: "#6f7888"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
            }
        }
    }
}

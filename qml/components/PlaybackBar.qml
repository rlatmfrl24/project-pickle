import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root

    property var media: ({})
    property var playbackController: null
    property var appController: null
    property var mediaPlayer: null
    property var audioOutput: null
    property bool scrubbing: false
    property real scrubPosition: 0

    readonly property bool hasPlayer: mediaPlayer !== null
    readonly property bool hasPlaybackError: hasPlayer
        && (mediaPlayer.error !== MediaPlayer.NoError
            || mediaPlayer.mediaStatus === MediaPlayer.InvalidMedia)
    readonly property bool canPlay: playbackController && playbackController.hasSource
        && hasPlayer && !hasPlaybackError
    readonly property bool isPlaying: canPlay && mediaPlayer.playbackState === MediaPlayer.PlayingState
    readonly property bool canSeek: canPlay && mediaPlayer.duration > 0 && mediaPlayer.seekable
    readonly property bool canStop: canPlay
        && (mediaPlayer.playbackState !== MediaPlayer.StoppedState || mediaPlayer.position > 0)
    readonly property bool canCaptureSnapshot: root.appController
        && root.appController.databaseReady
        && root.playbackController
        && root.playbackController.hasSource
        && root.hasPlayer
        && !root.appController.scanInProgress
        && !root.appController.metadataInProgress
        && !root.appController.snapshotInProgress
        && !root.appController.thumbnailMaintenanceInProgress

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
            root.persistPlaybackPosition()
            return
        }

        if (root.mediaPlayer.mediaStatus === MediaPlayer.EndOfMedia) {
            root.mediaPlayer.position = 0
        }

        root.mediaPlayer.play()
    }

    function stopPlayback() {
        if (!root.canPlay) {
            return
        }

        root.persistPlaybackPosition()
        root.mediaPlayer.stop()
        if (root.mediaPlayer.duration > 0 && root.mediaPlayer.seekable) {
            root.mediaPlayer.position = 0
        }
        root.scrubbing = false
    }

    function seekBy(deltaMs) {
        if (!root.canSeek) {
            return
        }

        const targetPosition = Math.max(0, Math.min(root.mediaPlayer.duration, root.mediaPlayer.position + deltaMs))
        root.mediaPlayer.position = Math.round(targetPosition)
    }

    function persistPlaybackPosition() {
        if (!root.appController || !root.canPlay) {
            return
        }

        const duration = root.mediaPlayer.duration > 0 ? root.mediaPlayer.duration : root.durationMs()
        let position = root.mediaPlayer.position > 0 ? root.mediaPlayer.position : 0
        if (duration > 0 && position >= duration - 5000) {
            position = 0
        }

        root.appController.saveSelectedPlaybackPosition(Math.round(position))
    }

    function playbackStatusText() {
        if (!root.playbackController || !root.playbackController.hasSource) {
            return qsTr("No selection")
        }

        if (!root.hasPlayer) {
            return qsTr("Player unavailable")
        }

        if (root.hasPlaybackError) {
            return qsTr("Playback error")
        }

        if (root.mediaPlayer.mediaStatus === MediaPlayer.LoadingMedia
                || root.mediaPlayer.mediaStatus === MediaPlayer.BufferingMedia
                || root.mediaPlayer.mediaStatus === MediaPlayer.StalledMedia) {
            return qsTr("Loading")
        }

        if (root.mediaPlayer.mediaStatus === MediaPlayer.EndOfMedia) {
            return qsTr("Ended")
        }

        if (root.mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            return qsTr("Playing")
        }

        if (root.mediaPlayer.playbackState === MediaPlayer.PausedState) {
            return qsTr("Paused")
        }

        return qsTr("Ready")
    }

    radius: 8
    color: "#171b24"
    border.color: "#283040"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: root.isPlaying ? qsTr("Pause") : qsTr("Play")
                enabled: root.canPlay
                onClicked: root.togglePlayback()
                Layout.preferredWidth: 72
            }

            Button {
                text: qsTr("Stop")
                enabled: root.canStop
                onClicked: root.stopPlayback()
                Layout.preferredWidth: 58
            }

            Button {
                text: qsTr("-5s")
                enabled: root.canSeek
                onClicked: root.seekBy(-5000)
                Layout.preferredWidth: 48
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

            Button {
                text: qsTr("+5s")
                enabled: root.canSeek
                onClicked: root.seekBy(5000)
                Layout.preferredWidth: 48
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            CheckBox {
                text: qsTr("Mute")
                checked: root.audioOutput ? root.audioOutput.muted : false
                enabled: root.audioOutput !== null
                onToggled: {
                    if (root.audioOutput) {
                        root.audioOutput.muted = checked
                    }
                    if (root.appController) {
                        root.appController.playerMuted = checked
                    }
                }
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
                        if (value > 0) {
                            root.audioOutput.muted = false
                        }
                    }
                    if (root.appController) {
                        root.appController.playerVolume = value / 100
                        if (value > 0) {
                            root.appController.playerMuted = false
                        }
                    }
                }
                Layout.preferredWidth: 120
            }

            Button {
                text: root.appController && root.appController.snapshotInProgress ? qsTr("Capturing") : qsTr("Snapshot")
                enabled: root.canCaptureSnapshot || (root.appController && root.appController.snapshotInProgress)
                onClicked: {
                    if (root.appController.snapshotInProgress) {
                        root.appController.cancelSnapshotCapture()
                    } else {
                        root.appController.captureSelectedSnapshot(Math.round(root.hasPlayer ? root.mediaPlayer.position : 0))
                    }
                }
                Layout.preferredWidth: 92
            }

            Label {
                text: root.audioOutput ? Math.round(root.audioOutput.volume * 100) + "%" : qsTr("--")
                color: "#7f8898"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 42
            }

            Label {
                text: root.playbackStatusText()
                color: root.hasPlaybackError ? "#ffb4a8" : "#7f8898"
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var media: ({})
    property var appController: null
    property int draftMediaId: -1
    property var draftTags: []
    property bool syncingFlags: false
    readonly property bool hasSelection: root.media && root.media.id !== undefined && root.media.id > 0
    readonly property var selectedSnapshots: root.appController ? root.appController.selectedSnapshots : []
    readonly property var reviewStatusKeys: ["unreviewed", "reviewed", "needs_followup"]
    readonly property var reviewStatusLabels: [qsTr("Unreviewed"), qsTr("Reviewed"), qsTr("Needs follow-up")]

    signal renameRequested()

    function displayValue(key, fallbackText) {
        if (!root.media) {
            return fallbackText
        }

        const value = root.media[key]
        return value === undefined || value === null || value === "" ? fallbackText : value
    }

    function imageSource(path) {
        if (!path || path.length === 0) {
            return ""
        }

        if (path.indexOf("file:") === 0 || path.indexOf("qrc:") === 0) {
            return path
        }

        return "file:///" + path.replace(/\\/g, "/")
    }

    function statusIndex(status) {
        const index = root.reviewStatusKeys.indexOf(status)
        return index >= 0 ? index : 0
    }

    function resetDraft() {
        root.draftMediaId = root.hasSelection ? root.media.id : -1
        descriptionEdit.text = root.displayValue("description", "")
        statusCombo.currentIndex = root.statusIndex(root.displayValue("reviewStatus", "unreviewed"))
        const ratingValue = Number(root.displayValue("rating", 0))
        ratingSpin.value = Number.isFinite(ratingValue) ? ratingValue : 0
        root.syncingFlags = true
        favoriteCheck.checked = Boolean(root.displayValue("isFavorite", false))
        deleteCandidateCheck.checked = Boolean(root.displayValue("isDeleteCandidate", false))
        root.syncingFlags = false
        const currentTags = root.media && root.media.tags ? root.media.tags : []
        const nextTags = []
        for (let i = 0; i < currentTags.length; ++i) {
            nextTags.push(String(currentTags[i]))
        }
        root.draftTags = nextTags
        tagInput.text = ""
    }

    function addTag(tagText) {
        const trimmedTag = tagText.trim()
        if (trimmedTag.length === 0) {
            return
        }

        const lowerTag = trimmedTag.toLocaleLowerCase()
        for (let i = 0; i < root.draftTags.length; ++i) {
            if (String(root.draftTags[i]).toLocaleLowerCase() === lowerTag) {
                tagInput.text = ""
                return
            }
        }

        const nextTags = root.draftTags.slice()
        nextTags.push(trimmedTag)
        root.draftTags = nextTags
        tagInput.text = ""
    }

    function removeTag(index) {
        const nextTags = root.draftTags.slice()
        nextTags.splice(index, 1)
        root.draftTags = nextTags
    }

    function saveDraft() {
        if (!root.appController || !root.hasSelection) {
            return
        }

        root.appController.saveSelectedMediaDetails(
            descriptionEdit.text,
            root.reviewStatusKeys[statusCombo.currentIndex],
            ratingSpin.value,
            root.draftTags)
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
        return hours > 0 ? hours + ":" + paddedMinutes + ":" + paddedSeconds : minutes + ":" + paddedSeconds
    }

    onMediaChanged: resetDraft()
    Component.onCompleted: resetDraft()

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
            spacing: 8

            Label {
                text: qsTr("Details")
                color: "#f3f6fb"
                font.pixelSize: 17
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            BusyIndicator {
                running: root.appController && root.appController.metadataInProgress
                visible: running
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }

            Button {
                text: qsTr("Rename")
                enabled: root.hasSelection
                    && root.appController
                    && !root.appController.scanInProgress
                    && !root.appController.metadataInProgress
                    && !root.appController.snapshotInProgress
                    && !root.appController.thumbnailMaintenanceInProgress
                onClicked: root.renameRequested()
                Layout.preferredWidth: 82
            }

            Button {
                text: qsTr("Refresh")
                enabled: root.hasSelection
                    && root.appController
                    && root.appController.databaseReady
                    && !root.appController.scanInProgress
                    && !root.appController.metadataInProgress
                    && !root.appController.snapshotInProgress
                    && !root.appController.thumbnailMaintenanceInProgress
                onClicked: root.appController.refreshSelectedMetadata()
                Layout.preferredWidth: 82
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Refresh Metadata")
            }
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

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appController
                ? [root.appController.metadataStatus, root.appController.snapshotStatus, root.appController.thumbnailMaintenanceStatus, root.appController.detailStatus, root.appController.fileActionStatus].filter(function(status) {
                    return status && status.length > 0
                }).join("  /  ")
                : ""
            color: text.indexOf("failed") >= 0 || text.indexOf("Invalid") >= 0 ? "#ffb4a8" : "#7f8898"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 10

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 12

                    Label { text: qsTr("Size"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("fileSize", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Modified"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("modifiedAt", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Duration"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("duration", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Resolution"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("resolution", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Codec"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("codec", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Bitrate"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("bitrate", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Frame rate"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("frameRate", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Last position"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.formatTime(Number(root.displayValue("lastPositionMs", 0)))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Label { text: qsTr("Last played"); color: "#7f8898"; font.pixelSize: 12 }
                    Label {
                        Layout.fillWidth: true
                        text: root.displayValue("lastPlayedAt", qsTr("-"))
                        color: "#c7d0de"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.displayValue("thumbnailPath", "").length > 0 ? 132 : 0
                    visible: height > 0
                    radius: 6
                    color: "#101620"
                    border.color: "#283040"
                    border.width: 1
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 8
                        source: root.imageSource(root.displayValue("thumbnailPath", ""))
                        sourceSize.width: 320
                        sourceSize.height: 180
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: true
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 12
                        anchors.bottomMargin: 10
                        text: qsTr("Thumbnail")
                        color: "#dfe7f4"
                        font.pixelSize: 12
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Snapshots") + ": " + root.selectedSnapshots.length
                    color: "#7f8898"
                    font.pixelSize: 12
                    visible: root.hasSelection
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#283040"
                }

                Label {
                    text: qsTr("Description")
                    color: "#7f8898"
                    font.pixelSize: 12
                }

                TextArea {
                    id: descriptionEdit

                    enabled: root.hasSelection
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("No description")
                    color: "#dfe7f4"
                    selectedTextColor: "#07111f"
                    selectionColor: "#8fc7ff"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 86
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    CheckBox {
                        id: favoriteCheck

                        text: qsTr("Favorite")
                        enabled: root.hasSelection
                            && root.appController
                            && root.appController.databaseReady
                            && !root.appController.scanInProgress
                            && !root.appController.metadataInProgress
                            && !root.appController.snapshotInProgress
                            && !root.appController.thumbnailMaintenanceInProgress
                        onToggled: {
                            if (!root.syncingFlags && root.appController) {
                                root.appController.setSelectedFavorite(checked)
                            }
                        }
                    }

                    CheckBox {
                        id: deleteCandidateCheck

                        text: qsTr("Delete candidate")
                        enabled: root.hasSelection
                            && root.appController
                            && root.appController.databaseReady
                            && !root.appController.scanInProgress
                            && !root.appController.metadataInProgress
                            && !root.appController.snapshotInProgress
                            && !root.appController.thumbnailMaintenanceInProgress
                        onToggled: {
                            if (!root.syncingFlags && root.appController) {
                                root.appController.setSelectedDeleteCandidate(checked)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: qsTr("Status")
                        color: "#7f8898"
                        font.pixelSize: 12
                    }

                    ComboBox {
                        id: statusCombo

                        enabled: root.hasSelection
                        model: root.reviewStatusLabels
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Rating")
                        color: "#7f8898"
                        font.pixelSize: 12
                    }

                    SpinBox {
                        id: ratingSpin

                        enabled: root.hasSelection
                        from: 0
                        to: 5
                        editable: false
                        Layout.preferredWidth: 76
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: tagInput

                        enabled: root.hasSelection
                        placeholderText: qsTr("Tag")
                        onAccepted: root.addTag(text)
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Add")
                        enabled: root.hasSelection && tagInput.text.trim().length > 0
                        onClicked: root.addTag(tagInput.text)
                        Layout.preferredWidth: 64
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: root.draftTags

                        delegate: Rectangle {
                            id: tag

                            required property int index
                            required property string modelData

                            height: 28
                            width: tagLabel.implicitWidth + removeTagButton.implicitWidth + 22
                            radius: 14
                            color: "#243348"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 4
                                spacing: 4

                                Label {
                                    id: tagLabel

                                    text: tag.modelData
                                    color: "#cfe2ff"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 130
                                }

                                ToolButton {
                                    id: removeTagButton

                                    text: qsTr("x")
                                    enabled: root.hasSelection
                                    onClicked: root.removeTag(tag.index)
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: qsTr("Save")
                        enabled: root.hasSelection
                            && root.appController
                            && root.appController.databaseReady
                            && !root.appController.scanInProgress
                            && !root.appController.metadataInProgress
                            && !root.appController.snapshotInProgress
                            && !root.appController.thumbnailMaintenanceInProgress
                        onClicked: root.saveDraft()
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Cancel")
                        enabled: root.hasSelection
                        onClicked: root.resetDraft()
                        Layout.fillWidth: true
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
    }
}

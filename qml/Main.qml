import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import "components" as Components
import "pages" as Pages

ApplicationWindow {
    id: window

    required property var mediaLibraryModel
    required property var appController
    required property var playbackController
    property string pendingRenameBaseName: ""
    property string diagnosticText: ""

    minimumWidth: 960
    minimumHeight: 600
    width: 1180
    height: 720
    visible: true
    title: qsTr("Pickle")
    color: "#111318"

    function baseNameForMedia(media) {
        const fileName = media && media.fileName ? String(media.fileName) : ""
        const dotIndex = fileName.lastIndexOf(".")
        return dotIndex > 0 ? fileName.slice(0, dotIndex) : fileName
    }

    function openRenameDialog(index) {
        if (index < 0) {
            return
        }

        window.appController.selectIndex(index)
        renameNameField.text = window.baseNameForMedia(window.appController.selectedMedia)
        renameDialog.open()
    }

    function hasActiveWork() {
        return window.appController.scanInProgress
            || window.appController.metadataInProgress
            || window.appController.snapshotInProgress
            || window.appController.thumbnailMaintenanceInProgress
    }

    function aboutText() {
        const info = window.appController.aboutInfo()
        return qsTr("Pickle") + "\n"
            + qsTr("Version") + ": " + info.applicationVersion + "\n"
            + qsTr("Qt") + ": " + info.qtVersion + "\n"
            + qsTr("Database") + ": " + (info.databasePath || qsTr("-"))
    }

    function systemInfoText() {
        const info = window.appController.systemInfo()
        return qsTr("Application") + ": " + info.applicationName + " " + info.applicationVersion + "\n"
            + qsTr("Qt") + ": " + info.qtVersion + "\n"
            + qsTr("OS") + ": " + info.os + "\n"
            + qsTr("Kernel") + ": " + info.kernelType + " " + info.kernelVersion + "\n"
            + qsTr("CPU") + ": " + info.cpuArchitecture + "\n"
            + qsTr("Build ABI") + ": " + info.buildAbi + "\n"
            + qsTr("Host") + ": " + info.hostName + "\n"
            + qsTr("Database ready") + ": " + info.databaseReady + "\n"
            + qsTr("Database status") + ": " + info.databaseStatus + "\n"
            + qsTr("Database path") + ": " + (info.databasePath || qsTr("-")) + "\n"
            + qsTr("Scan root") + ": " + (info.currentScanRoot || qsTr("-")) + "\n"
            + qsTr("Library items") + ": " + info.libraryItems + "\n"
            + qsTr("Scan in progress") + ": " + info.scanInProgress + "\n"
            + qsTr("Metadata in progress") + ": " + info.metadataInProgress + "\n"
            + qsTr("Snapshot in progress") + ": " + info.snapshotInProgress + "\n"
            + qsTr("Thumbnail rebuild in progress") + ": " + info.thumbnailMaintenanceInProgress + "\n"
            + qsTr("Snapshot root") + ": " + (info.snapshotRoot || qsTr("-")) + "\n"
            + qsTr("Thumbnail root") + ": " + (info.thumbnailRoot || qsTr("-")) + "\n"
            + qsTr("App data") + ": " + (info.appDataPath || qsTr("-")) + "\n"
            + qsTr("Log path") + ": " + (info.logPath || qsTr("-")) + "\n"
            + qsTr("Rotated log") + ": " + (info.rotatedLogPath || qsTr("-")) + "\n"
            + qsTr("Tools") + ": " + (info.toolsStatus || qsTr("-"))
    }

    function openSettingsDialog() {
        ffprobePathField.text = window.appController.ffprobePath
        ffmpegPathField.text = window.appController.ffmpegPath
        settingsShowThumbnails.checked = window.appController.showThumbnails
        settingsSortCombo.currentIndex = window.appController.librarySortKey === "size" ? 1
            : (window.appController.librarySortKey === "modified" ? 2 : 0)
        settingsSortAscending.checked = window.appController.librarySortAscending
        settingsVolumeSlider.value = Math.round(window.appController.playerVolume * 100)
        settingsMutedCheck.checked = window.appController.playerMuted
        settingsDialog.open()
    }

    function saveSettingsDialogValues() {
        window.appController.saveSettings({
            "ffprobePath": ffprobePathField.text,
            "ffmpegPath": ffmpegPathField.text,
            "showThumbnails": settingsShowThumbnails.checked,
            "sortKey": settingsSortCombo.currentIndex === 1 ? "size" : (settingsSortCombo.currentIndex === 2 ? "modified" : "name"),
            "sortAscending": settingsSortAscending.checked,
            "playerVolume": settingsVolumeSlider.value / 100,
            "playerMuted": settingsMutedCheck.checked
        })
    }

    function refreshDiagnosticText() {
        window.diagnosticText = window.appController.diagnosticReport()
    }

    onClosing: playerPage.persistPlaybackPosition()

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")

            MenuItem {
                text: qsTr("Open Folder...")
                enabled: window.appController.databaseReady && !window.hasActiveWork()
                onTriggered: folderDialog.open()
            }

            MenuItem {
                text: qsTr("Cancel Active Work")
                enabled: window.hasActiveWork()
                onTriggered: window.appController.cancelActiveWork()
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Quit")
                onTriggered: window.close()
            }
        }

        Menu {
            title: qsTr("&Library")

            MenuItem {
                text: qsTr("Rescan Current Root")
                enabled: window.appController.databaseReady
                    && !window.hasActiveWork()
                    && window.appController.currentScanRoot.length > 0
                onTriggered: window.appController.rescanCurrentRoot()
            }

            MenuItem {
                text: qsTr("Cancel Scan")
                enabled: window.appController.scanCancelAvailable
                onTriggered: window.appController.cancelScan()
            }

            MenuItem {
                text: window.appController.metadataInProgress ? qsTr("Reading Metadata") : qsTr("Refresh Selected Metadata")
                enabled: window.appController.databaseReady
                    && window.appController.selectedIndex >= 0
                    && !window.hasActiveWork()
                onTriggered: window.appController.refreshSelectedMetadata()
            }

            MenuItem {
                text: qsTr("Cancel Metadata Refresh")
                enabled: window.appController.metadataInProgress
                onTriggered: window.appController.cancelMetadataRefresh()
            }

            MenuItem {
                text: window.appController.snapshotInProgress ? qsTr("Capturing Snapshot") : qsTr("Capture Snapshot")
                enabled: window.appController.databaseReady
                    && window.appController.selectedIndex >= 0
                    && window.playbackController.hasSource
                    && !window.hasActiveWork()
                onTriggered: window.appController.captureSelectedSnapshot(Math.round(playerPage.mediaPlayer.position))
            }

            MenuItem {
                text: qsTr("Cancel Snapshot")
                enabled: window.appController.snapshotInProgress
                onTriggered: window.appController.cancelSnapshotCapture()
            }

            MenuItem {
                text: window.appController.thumbnailMaintenanceInProgress ? qsTr("Rebuilding Thumbnail Cache") : qsTr("Rebuild Thumbnail Cache")
                enabled: window.appController.databaseReady && !window.hasActiveWork()
                onTriggered: window.appController.rebuildThumbnailCache()
            }

            MenuItem {
                text: qsTr("Cancel Thumbnail Rebuild")
                enabled: window.appController.thumbnailMaintenanceInProgress
                onTriggered: window.appController.cancelThumbnailMaintenance()
            }

            MenuItem {
                text: qsTr("Rename Selected...")
                enabled: window.appController.selectedIndex >= 0
                    && !window.hasActiveWork()
                onTriggered: window.openRenameDialog(window.appController.selectedIndex)
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Reset Library...")
                enabled: window.appController.databaseReady
                    && !window.hasActiveWork()
                onTriggered: resetLibraryDialog.open()
            }
        }

        Menu {
            title: qsTr("&View")

            MenuItem {
                text: qsTr("Show Thumbnails")
                checkable: true
                checked: window.appController.showThumbnails
                onTriggered: window.appController.showThumbnails = checked
            }
        }

        Menu {
            title: qsTr("&Tools")

            MenuItem {
                text: qsTr("Settings...")
                onTriggered: window.openSettingsDialog()
            }

            MenuItem {
                text: qsTr("Diagnostics...")
                onTriggered: {
                    window.refreshDiagnosticText()
                    diagnosticsDialog.open()
                }
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Open Log File")
                onTriggered: window.appController.openLogFile()
            }

            MenuItem {
                text: qsTr("Open App Data Folder")
                onTriggered: window.appController.openAppDataFolder()
            }
        }

        Menu {
            title: qsTr("&Help")

            MenuItem {
                text: qsTr("Version")
                onTriggered: aboutDialog.open()
            }

            MenuItem {
                text: qsTr("System Information")
                onTriggered: systemInfoDialog.open()
            }
        }
    }

    FolderDialog {
        id: folderDialog

        title: qsTr("Select media folder")
        currentFolder: window.appController.lastOpenFolderUrl
        onAccepted: window.appController.startDirectoryScan(selectedFolder)
    }

    FileDialog {
        id: ffprobeFileDialog

        title: qsTr("Select ffprobe executable")
        fileMode: FileDialog.OpenFile
        onAccepted: ffprobePathField.text = window.appController.localPathFromUrl(selectedFile)
    }

    FileDialog {
        id: ffmpegFileDialog

        title: qsTr("Select ffmpeg executable")
        fileMode: FileDialog.OpenFile
        onAccepted: ffmpegPathField.text = window.appController.localPathFromUrl(selectedFile)
    }

    Dialog {
        id: resetLibraryDialog

        title: qsTr("Reset Library")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Math.min(window.width - 40, 460)
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        onAccepted: {
            playerPage.releaseLoadedSource()
            window.appController.resetLibrary()
        }

        Label {
            width: parent ? parent.width : 420
            text: qsTr("This clears scanned library records, tags, snapshots, and scan roots from the database. It does not delete video files from disk.")
            color: "#dfe7f4"
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: aboutDialog

        title: qsTr("Version")
        modal: true
        standardButtons: Dialog.Ok
        width: Math.min(window.width - 40, 440)
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)

        Label {
            width: parent ? parent.width : 400
            text: window.aboutText()
            color: "#dfe7f4"
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: systemInfoDialog

        title: qsTr("System Information")
        modal: true
        standardButtons: Dialog.Ok
        width: Math.min(window.width - 40, 560)
        height: Math.min(window.height - 80, 420)
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)

        ScrollView {
            anchors.fill: parent
            clip: true

            TextArea {
                text: window.systemInfoText()
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: "#dfe7f4"
            }
        }
    }

    Dialog {
        id: settingsDialog

        title: qsTr("Settings")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Math.min(window.width - 40, 620)
        height: Math.min(window.height - 80, 620)
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        onAccepted: window.saveSettingsDialogValues()

        ScrollView {
            anchors.fill: parent
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: qsTr("External tools")
                    color: "#f3f6fb"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8

                    Label {
                        text: qsTr("ffprobe")
                        color: "#aeb7c7"
                    }

                    TextField {
                        id: ffprobePathField

                        Layout.fillWidth: true
                        placeholderText: qsTr("Use PATH")
                        selectByMouse: true
                    }

                    Button {
                        text: qsTr("Browse")
                        onClicked: ffprobeFileDialog.open()
                    }

                    Label {
                        text: qsTr("ffmpeg")
                        color: "#aeb7c7"
                    }

                    TextField {
                        id: ffmpegPathField

                        Layout.fillWidth: true
                        placeholderText: qsTr("Use PATH")
                        selectByMouse: true
                    }

                    Button {
                        text: qsTr("Browse")
                        onClicked: ffmpegFileDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: qsTr("Use PATH")
                        onClicked: {
                            ffprobePathField.text = ""
                            ffmpegPathField.text = ""
                            window.appController.resetToolPathsToPath()
                        }
                    }

                    Button {
                        text: qsTr("Test Tools")
                        onClicked: {
                            window.saveSettingsDialogValues()
                            window.appController.validateExternalTools()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: window.appController.toolsStatus
                        color: window.appController.toolsStatus.indexOf("unavailable") >= 0 ? "#ffb4a8" : "#8fd19e"
                        elide: Text.ElideRight
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Library")
                    color: "#f3f6fb"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    CheckBox {
                        id: settingsShowThumbnails

                        text: qsTr("Show thumbnails")
                    }

                    ComboBox {
                        id: settingsSortCombo

                        model: [qsTr("Name"), qsTr("Size"), qsTr("Modified")]
                        Layout.preferredWidth: 140
                    }

                    CheckBox {
                        id: settingsSortAscending

                        text: qsTr("Ascending")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Player")
                    color: "#f3f6fb"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    CheckBox {
                        id: settingsMutedCheck

                        text: qsTr("Muted")
                    }

                    Label {
                        text: qsTr("Volume")
                        color: "#aeb7c7"
                    }

                    Slider {
                        id: settingsVolumeSlider

                        from: 0
                        to: 100
                        stepSize: 1
                        Layout.fillWidth: true
                    }

                    Label {
                        text: Math.round(settingsVolumeSlider.value) + "%"
                        color: "#aeb7c7"
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 44
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: window.appController.settingsStatus
                    color: window.appController.settingsStatus.indexOf("failed") >= 0 ? "#ffb4a8" : "#8e97a8"
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Dialog {
        id: diagnosticsDialog

        title: qsTr("Diagnostics")
        modal: true
        standardButtons: Dialog.Ok
        width: Math.min(window.width - 40, 700)
        height: Math.min(window.height - 80, 560)
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: qsTr("Refresh")
                    onClicked: window.refreshDiagnosticText()
                }

                Button {
                    text: qsTr("Copy")
                    onClicked: {
                        diagnosticsTextArea.selectAll()
                        diagnosticsTextArea.copy()
                    }
                }

                Button {
                    text: qsTr("Open Log")
                    onClicked: window.appController.openLogFile()
                }

                Button {
                    text: qsTr("Open Folder")
                    onClicked: window.appController.openAppDataFolder()
                }

                Button {
                    text: qsTr("Clear Events")
                    onClicked: {
                        window.appController.clearWorkEvents()
                        window.refreshDiagnosticText()
                    }
                }
            }

            TextArea {
                id: diagnosticsTextArea

                Layout.fillWidth: true
                Layout.fillHeight: true
                text: window.diagnosticText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: "#dfe7f4"
            }
        }
    }

    Dialog {
        id: renameDialog

        title: qsTr("Rename Media")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Math.min(window.width - 40, 420)
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        onOpened: {
            renameNameField.forceActiveFocus()
            renameNameField.selectAll()
        }
        onAccepted: {
            window.pendingRenameBaseName = renameNameField.text
            playerPage.releaseLoadedSource()
            renameAfterReleaseTimer.restart()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("New name")
                color: "#dfe7f4"
            }

            TextField {
                id: renameNameField

                Layout.fillWidth: true
                selectByMouse: true
                onAccepted: renameDialog.accept()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The original extension will be kept.")
                color: "#7f8898"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
    }

    Timer {
        id: renameAfterReleaseTimer

        interval: 200
        repeat: false
        onTriggered: {
            window.appController.renameSelectedMedia(window.pendingRenameBaseName)
            playerPage.releaseLoadedSource(false)
            window.pendingRenameBaseName = ""
        }
    }

    header: ToolBar {
        background: Rectangle {
            color: "#151922"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 14

            Label {
                text: qsTr("Pickle")
                color: "#f3f6fb"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Label {
                text: qsTr("Library review workspace")
                color: "#8e97a8"
                font.pixelSize: 13
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Label {
                text: window.appController.databaseStatus
                color: window.appController.databaseReady ? "#8fd19e" : "#ffb4a8"
                font.pixelSize: 12
                elide: Text.ElideMiddle
                Layout.maximumWidth: 220
            }

            Label {
                text: window.appController.scanInProgress && window.appController.scanProgressText.length > 0
                    ? window.appController.scanProgressText
                    : window.appController.scanStatus
                visible: text.length > 0
                color: window.appController.scanInProgress ? "#9fc9ff" : "#8e97a8"
                font.pixelSize: 12
                elide: Text.ElideMiddle
                Layout.maximumWidth: 260
            }

            Label {
                text: window.appController.thumbnailMaintenanceStatus
                visible: text.length > 0
                color: window.appController.thumbnailMaintenanceInProgress ? "#9fc9ff" : "#8e97a8"
                font.pixelSize: 12
                elide: Text.ElideMiddle
                Layout.maximumWidth: 260
            }

            Button {
                text: window.appController.scanInProgress ? qsTr("Cancel") : qsTr("Rescan")
                enabled: window.appController.databaseReady
                    && (window.appController.scanInProgress
                        || (!window.hasActiveWork() && window.appController.currentScanRoot.length > 0))
                onClicked: {
                    if (window.appController.scanInProgress) {
                        window.appController.cancelScan()
                    } else {
                        window.appController.rescanCurrentRoot()
                    }
                }
            }

            Button {
                text: qsTr("Open Folder")
                enabled: window.appController.databaseReady && !window.hasActiveWork()
                onClicked: folderDialog.open()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#111318"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 560
                spacing: 14

                Pages.PlayerPage {
                    id: playerPage

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    media: window.appController.selectedMedia
                    playbackController: window.playbackController
                    appController: window.appController
                }

                Components.PlaybackBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    media: window.appController.selectedMedia
                    playbackController: window.playbackController
                    appController: window.appController
                    mediaPlayer: playerPage.mediaPlayer
                    audioOutput: playerPage.audioOutput
                    playerPage: playerPage
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 372
                Layout.minimumWidth: 320
                Layout.maximumWidth: 430
                Layout.fillHeight: true
                spacing: 14

                Components.LibraryPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 430
                    model: window.mediaLibraryModel
                    currentIndex: window.appController.selectedIndex
                    searchText: window.appController.librarySearchText
                    sortKey: window.appController.librarySortKey
                    sortAscending: window.appController.librarySortAscending
                    showThumbnails: window.appController.showThumbnails
                    libraryStatus: window.appController.libraryStatus
                    onSelected: function(index) {
                        playerPage.persistPlaybackPosition()
                        window.appController.selectIndex(index)
                    }
                    onActivated: function(index) {
                        playerPage.persistPlaybackPosition()
                        window.appController.selectIndex(index)
                        playerPage.playLoadedSource()
                    }
                    onRenameRequested: function(index) {
                        playerPage.persistPlaybackPosition()
                        window.openRenameDialog(index)
                    }
                    onSearchRequested: function(searchText) {
                        playerPage.persistPlaybackPosition()
                        window.appController.librarySearchText = searchText
                    }
                    onSortKeyRequested: function(sortKey) {
                        playerPage.persistPlaybackPosition()
                        window.appController.librarySortKey = sortKey
                    }
                    onSortAscendingRequested: function(ascending) {
                        playerPage.persistPlaybackPosition()
                        window.appController.librarySortAscending = ascending
                    }
                    onThumbnailVisibilityRequested: function(showThumbnails) {
                        window.appController.showThumbnails = showThumbnails
                    }
                }

                Components.InfoPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 240
                    media: window.appController.selectedMedia
                    appController: window.appController
                    onRenameRequested: {
                        playerPage.persistPlaybackPosition()
                        window.openRenameDialog(window.appController.selectedIndex)
                    }
                }
            }
        }
    }
}

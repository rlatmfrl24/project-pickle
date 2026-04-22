import QtQuick
import QtQuick.Window

Item {
    id: root

    property bool reviewMode: false
    property bool modalBlocked: false
    property var appController: null
    property var playerPage: null

    signal toggleReviewModeRequested()

    readonly property bool inputFocusBlocked: root.focusItemBlocksShortcuts(Window.window ? Window.window.activeFocusItem : null)
    readonly property bool toggleEnabled: !root.modalBlocked && !root.inputFocusBlocked
    readonly property bool actionEnabled: root.reviewMode
        && root.toggleEnabled
        && root.appController
        && root.appController.databaseReady
        && root.appController.selectedIndex >= 0

    function focusItemBlocksShortcuts(item) {
        if (!item || item === root) {
            return false
        }

        if (item["echoMode"] !== undefined
                || item["placeholderText"] !== undefined
                || (item["selectedText"] !== undefined && item["cursorPosition"] !== undefined)) {
            return true
        }

        if (item["editable"] !== undefined && item["currentIndex"] !== undefined) {
            return true
        }

        if (item["from"] !== undefined && item["to"] !== undefined && item["value"] !== undefined) {
            return true
        }

        return false
    }

    function normalizePrimarySelection() {
        if (!root.appController || root.appController.selectedIndex < 0) {
            return false
        }

        if (root.appController.selectedMediaCount > 1) {
            root.appController.selectIndex(root.appController.selectedIndex)
        }

        return true
    }

    function moveSelection(offset) {
        if (!root.actionEnabled || !root.appController) {
            return
        }

        if (root.playerPage) {
            root.playerPage.persistPlaybackPosition()
        }

        root.appController.selectRelative(offset)
    }

    function togglePlayback() {
        if (root.actionEnabled && root.playerPage) {
            root.playerPage.togglePlayback()
        }
    }

    function seekBy(deltaMs) {
        if (root.actionEnabled && root.playerPage && root.playerPage.canSeek) {
            root.playerPage.seekBy(deltaMs)
        }
    }

    function setRating(rating) {
        if (root.actionEnabled && root.normalizePrimarySelection()) {
            root.appController.setSelectedMediaRating(rating)
        }
    }

    function setReviewStatus(status) {
        if (root.actionEnabled && root.normalizePrimarySelection()) {
            root.appController.setSelectedMediaReviewStatus(status)
        }
    }

    function toggleFavorite() {
        if (!root.actionEnabled || !root.normalizePrimarySelection()) {
            return
        }

        const media = root.appController.selectedMedia || {}
        root.appController.setSelectedFavorite(!Boolean(media.isFavorite))
    }

    function toggleDeleteCandidate() {
        if (!root.actionEnabled || !root.normalizePrimarySelection()) {
            return
        }

        const media = root.appController.selectedMedia || {}
        root.appController.setSelectedDeleteCandidate(!Boolean(media.isDeleteCandidate))
    }

    function captureSnapshot() {
        if (!root.actionEnabled
                || !root.playerPage
                || !root.playerPage.hasSource
                || root.appController.snapshotInProgress) {
            return
        }

        root.appController.captureSelectedSnapshot(Math.round(root.playerPage.mediaPlayer.position))
    }

    Shortcut {
        sequence: "Ctrl+R"
        context: Qt.ApplicationShortcut
        enabled: root.toggleEnabled
        onActivated: root.toggleReviewModeRequested()
    }

    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.moveSelection(-1) }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.moveSelection(-1) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.moveSelection(1) }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.moveSelection(1) }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.togglePlayback() }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.seekBy(-5000) }
    Shortcut { sequence: "H"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.seekBy(-5000) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.seekBy(5000) }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.seekBy(5000) }
    Shortcut { sequence: "0"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setRating(0) }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setRating(1) }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setRating(2) }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setRating(3) }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setRating(4) }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setRating(5) }
    Shortcut { sequence: "U"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setReviewStatus("unreviewed") }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setReviewStatus("reviewed") }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.setReviewStatus("needs_followup") }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.toggleFavorite() }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.toggleDeleteCandidate() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: root.actionEnabled; onActivated: root.captureSnapshot() }
}

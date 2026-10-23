/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 * Copyright (C) 2026 Astra Media contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Util
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Player
import VLC.Playlist
import VLC.Dialogs

// Astra Media home: a large title, the "Now playing" card while something
// plays, and big tiles to start playing something.
FocusScope {
    id: root

    property var pagePrefix: [] // behave like a Page

    readonly property bool hasMedia: Player.isStarted
    readonly property bool isPlaying: Player.playingState === Player.PLAYING_STATE_PLAYING
    readonly property bool isDark: theme.palette.isDark

    readonly property real contentWidth: Math.min(width - VLCStyle.margin_xlarge * 2, VLCStyle.dp(980, VLCStyle.scale))

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Home View")

    function greeting() : string {
        const hour = new Date().getHours()
        if (hour >= 6 && hour < 13)
            return qsTr("Good morning")
        if (hour >= 13 && hour < 21)
            return qsTr("Good afternoon")
        return qsTr("Good evening")
    }

    // the visualizer of the card only runs while it can be seen
    Binding {
        target: AudioSpectrum
        property: "active"
        value: root.visible && root.hasMedia && root.isPlaying && !Player.hasVideoOutput
        restoreMode: Binding.RestoreValue
    }

    component Tile: T.AbstractButton {
        id: tile

        required property string glyph
        required property color tint
        property string subtitle

        implicitWidth: VLCStyle.dp(220, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(92, VLCStyle.scale)

        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        Accessible.name: text
        Accessible.description: subtitle

        scale: down ? VLCStyle.pressedScale : (hovered ? 1.02 : 1.0)
        Behavior on scale {
            NumberAnimation {
                duration: VLCStyle.duration_long
                easing.type: VLCStyle.easing_spring
                easing.overshoot: VLCStyle.spring_overshoot
            }
        }

        Keys.onPressed: (event) => {
            if (KeyHelper.matchOk(event)) {
                tile.clicked()
                event.accepted = true
            } else {
                tile.Navigation.defaultKeyAction(event)
            }
        }

        background: Rectangle {
            radius: VLCStyle.dp(16, VLCStyle.scale)
            color: root.isDark ? Qt.rgba(1, 1, 1, tile.hovered ? 0.09 : 0.06)
                               : Qt.rgba(1, 1, 1, tile.hovered ? 1.0 : 0.85)
            border.width: tile.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : VLCStyle.border
            border.color: tile.visualFocus ? theme.accent
                                           : (root.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06))

            Behavior on color { ColorAnimation { duration: VLCStyle.duration_short } }

            Widgets.RoundedRectangleShadow {
                visible: !root.isDark
                color: Qt.rgba(0, 0, 0, tile.hovered ? 0.10 : 0.05)
                yOffset: VLCStyle.dp(tile.hovered ? 8 : 3, VLCStyle.scale)
                blurRadius: VLCStyle.dp(tile.hovered ? 24 : 12, VLCStyle.scale)
            }
        }

        contentItem: RowLayout {
            spacing: VLCStyle.margin_small

            Rectangle {
                Layout.leftMargin: VLCStyle.margin_xsmall
                Layout.preferredWidth: VLCStyle.dp(44, VLCStyle.scale)
                Layout.preferredHeight: Layout.preferredWidth
                radius: VLCStyle.dp(11, VLCStyle.scale)

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(tile.tint, 1.18) }
                    GradientStop { position: 1.0; color: tile.tint }
                }

                Widgets.IconLabel {
                    anchors.centerIn: parent
                    text: tile.glyph
                    color: "white"
                    font.pixelSize: VLCStyle.dp(22, VLCStyle.scale)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.dp(2, VLCStyle.scale)

                T.Label {
                    Layout.fillWidth: true
                    text: tile.text
                    elide: Text.ElideRight
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_large
                    font.weight: Font.DemiBold
                }

                T.Label {
                    Layout.fillWidth: true
                    text: tile.subtitle
                    visible: text.length > 0
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                }
            }
        }

        padding: VLCStyle.margin_small
    }

    Flickable {
        id: flickable

        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + VLCStyle.margin_xlarge * 2
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout {
            id: column

            x: (flickable.width - width) / 2
            y: VLCStyle.margin_large
            width: root.contentWidth
            spacing: VLCStyle.margin_large

            // ── Large title ───────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.dp(2, VLCStyle.scale)

                T.Label {
                    text: root.greeting()
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_large
                    font.weight: Font.Medium
                }

                T.Label {
                    text: qsTr("Home")
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.dp(32, VLCStyle.scale)
                    font.weight: Font.Bold
                    font.letterSpacing: -0.5
                }
            }

            // ── Now playing card ──────────────────────────────────
            Item {
                id: nowPlaying

                Layout.fillWidth: true
                Layout.preferredHeight: root.hasMedia ? VLCStyle.dp(224, VLCStyle.scale) : 0
                visible: Layout.preferredHeight > 1
                opacity: root.hasMedia ? 1.0 : 0.0
                clip: !root.hasMedia

                Behavior on Layout.preferredHeight {
                    NumberAnimation { duration: VLCStyle.duration_veryLong; easing.type: VLCStyle.easing_emphasized }
                }
                Behavior on opacity {
                    NumberAnimation { duration: VLCStyle.duration_long }
                }

                readonly property url artwork: (Player.artwork && Player.artwork.toString())
                                               ? Player.artwork
                                               : (Player.hasVideoOutput ? VLCStyle.noArtVideoCover : VLCStyle.noArtAlbumCover)


                // soft, color-rich background made from a tiny version of the artwork
                Widgets.ImageExt {
                    id: cardBackground

                    anchors.fill: parent
                    source: nowPlaying.artwork
                    sourceSize: Qt.size(4, 4)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    radius: VLCStyle.dp(22, VLCStyle.scale)
                    backgroundColor: "#2C2C2E"
                }

                Rectangle {
                    anchors.fill: parent
                    radius: cardBackground.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.55) }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.38) }
                    }
                    border.width: VLCStyle.border
                    border.color: Qt.rgba(1, 1, 1, 0.10)
                }

                Widgets.RoundedRectangleShadow {
                    parent: cardBackground
                    color: Qt.rgba(0, 0, 0, root.isDark ? 0.35 : 0.18)
                    yOffset: VLCStyle.dp(14, VLCStyle.scale)
                    blurRadius: VLCStyle.dp(40, VLCStyle.scale)
                }

                // clicking anywhere on the card (but the buttons) opens the player
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: MainCtx.playerView = true
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_large
                    spacing: VLCStyle.margin_large

                    Widgets.ImageExt {
                        id: cardCover

                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: Player.hasVideoOutput ? parent.height * 16 / 9 : parent.height
                        Layout.maximumWidth: parent.width * 0.42

                        source: nowPlaying.artwork
                        sourceSize: Qt.size(512, 512)
                        fillMode: Image.PreserveAspectCrop
                        radius: VLCStyle.dp(14, VLCStyle.scale)
                        backgroundColor: "#1C1C1E"

                        scale: (root.isPlaying ? 1.0 : 0.94) * (1.0 + AudioSpectrum.bass * 0.03)
                        Behavior on scale {
                            NumberAnimation { duration: VLCStyle.duration_long; easing.type: VLCStyle.easing_standard }
                        }

                        Widgets.RoundedRectangleShadow {
                            color: Qt.rgba(0, 0, 0, 0.35)
                            yOffset: VLCStyle.dp(8, VLCStyle.scale)
                            blurRadius: VLCStyle.dp(24, VLCStyle.scale)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: VLCStyle.dp(4, VLCStyle.scale)

                        T.Label {
                            text: root.isPlaying ? qsTr("NOW PLAYING") : qsTr("PAUSED")
                            color: Qt.rgba(1, 1, 1, 0.7)
                            font.pixelSize: VLCStyle.fontSize_small
                            font.weight: Font.Bold
                            font.letterSpacing: 1.2
                        }

                        T.Label {
                            Layout.fillWidth: true
                            text: Player.title || Player.name
                            color: "white"
                            elide: Text.ElideRight
                            font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
                            font.weight: Font.Bold
                        }

                        T.Label {
                            Layout.fillWidth: true
                            text: [Player.artist, Player.album].filter((t) => !!t).join("  ·  ")
                            visible: text.length > 0
                            color: Qt.rgba(1, 1, 1, 0.75)
                            elide: Text.ElideRight
                            font.pixelSize: VLCStyle.fontSize_large
                        }

                        Item { Layout.fillHeight: true }

                        AstraSpectrumBars {
                            Layout.fillWidth: true
                            Layout.maximumWidth: VLCStyle.dp(300, VLCStyle.scale)
                            Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)
                            visible: !Player.hasVideoOutput
                            bands: 20
                            showPeaks: false
                            color: "white"
                            glowColor: "white"
                        }

                        RowLayout {
                            spacing: VLCStyle.margin_xsmall

                            Widgets.IconToolButton {
                                text: VLCIcons.previous
                                description: qsTr("Previous")
                                color: "white"
                                backgroundColor: "transparent"
                                font.pixelSize: VLCStyle.icon_normal
                                onClicked: MainPlaylistController.prev()
                            }

                            T.AbstractButton {
                                id: cardPlayButton

                                implicitWidth: VLCStyle.dp(46, VLCStyle.scale)
                                implicitHeight: implicitWidth
                                hoverEnabled: true
                                Accessible.name: root.isPlaying ? qsTr("Pause") : qsTr("Play")

                                scale: down ? VLCStyle.pressedScale : (hovered ? 1.06 : 1.0)
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: VLCStyle.duration_long
                                        easing.type: VLCStyle.easing_spring
                                        easing.overshoot: VLCStyle.spring_overshoot
                                    }
                                }

                                onClicked: MainPlaylistController.togglePlayPause()

                                background: Rectangle {
                                    radius: width / 2
                                    color: "white"
                                }

                                contentItem: Widgets.IconLabel {
                                    text: root.isPlaying ? VLCIcons.pause_filled : VLCIcons.play_filled
                                    color: "#1C1C1E"
                                    font.pixelSize: VLCStyle.dp(20, VLCStyle.scale)
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Widgets.IconToolButton {
                                text: VLCIcons.next
                                description: qsTr("Next")
                                color: "white"
                                backgroundColor: "transparent"
                                font.pixelSize: VLCStyle.icon_normal
                                onClicked: MainPlaylistController.next()
                            }

                            Item { Layout.preferredWidth: VLCStyle.margin_small }

                            T.AbstractButton {
                                id: openPlayerButton

                                text: qsTr("Open player")
                                hoverEnabled: true
                                padding: VLCStyle.margin_small
                                leftPadding: VLCStyle.margin_normal
                                rightPadding: VLCStyle.margin_normal

                                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                                implicitHeight: implicitContentHeight + topPadding + bottomPadding

                                onClicked: MainCtx.playerView = true

                                background: Rectangle {
                                    radius: height / 2
                                    color: Qt.rgba(1, 1, 1, openPlayerButton.hovered ? 0.28 : 0.18)
                                    border.width: VLCStyle.border
                                    border.color: Qt.rgba(1, 1, 1, 0.25)
                                    Behavior on color { ColorAnimation { duration: VLCStyle.duration_short } }
                                }

                                contentItem: T.Label {
                                    text: openPlayerButton.text
                                    color: "white"
                                    font.pixelSize: VLCStyle.fontSize_normal
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }
            }

            // ── Start ─────────────────────────────────────────────
            T.Label {
                Layout.topMargin: VLCStyle.margin_small
                text: root.hasMedia ? qsTr("Play something else") : qsTr("Get started")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_xxlarge
                font.weight: Font.Bold
            }

            GridLayout {
                id: tiles

                Layout.fillWidth: true
                columns: Math.max(1, Math.floor((root.contentWidth + columnSpacing) / (VLCStyle.dp(240, VLCStyle.scale) + columnSpacing)))
                columnSpacing: VLCStyle.margin_normal
                rowSpacing: VLCStyle.margin_normal

                Tile {
                    Layout.fillWidth: true
                    focus: true
                    text: qsTr("Open files")
                    subtitle: qsTr("Choose one or more files from your computer")
                    glyph: VLCIcons.ic_fluent_document_add_24_regular
                    tint: "#0A84FF"
                    onClicked: DialogsProvider.simpleOpenDialog()
                }

                Tile {
                    Layout.fillWidth: true
                    text: qsTr("Open a folder")
                    subtitle: qsTr("Play everything inside a folder")
                    glyph: VLCIcons.ic_fluent_folder_open_24_regular
                    tint: "#FF9F0A"
                    onClicked: DialogsProvider.PLOpenDir()
                }

                Tile {
                    Layout.fillWidth: true
                    text: qsTr("Network stream")
                    subtitle: qsTr("Play a link from the Internet")
                    glyph: VLCIcons.topbar_network
                    tint: "#30B0C7"
                    onClicked: DialogsProvider.openNetDialog()
                }

                Tile {
                    Layout.fillWidth: true
                    text: qsTr("Disc")
                    subtitle: qsTr("DVD, Blu-ray or audio CD")
                    glyph: VLCIcons.eject
                    tint: "#BF5AF2"
                    onClicked: DialogsProvider.openDiscDialog()
                }

                Tile {
                    Layout.fillWidth: true
                    text: qsTr("Capture device")
                    subtitle: qsTr("Webcam or capture card")
                    glyph: VLCIcons.record
                    tint: "#FF375F"
                    onClicked: DialogsProvider.openCaptureDialog()
                }

                Tile {
                    Layout.fillWidth: true
                    text: qsTr("Paste a link")
                    subtitle: qsTr("Play the address you copied")
                    glyph: VLCIcons.ic_fluent_document_copy_24_regular
                    tint: "#34C759"
                    onClicked: DialogsProvider.openUrlDialog()
                }
            }

            T.Label {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_xsmall
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Tip: drag files onto this window to play them right away.")
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                wrapMode: Text.WordWrap
            }
        }
    }
}

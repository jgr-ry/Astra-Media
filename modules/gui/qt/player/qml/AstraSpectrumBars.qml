/*****************************************************************************
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

import VLC.Style
import VLC.Player

// Astra Media visualizer: rounded bars that follow the real spectrum of the
// music. Bass sits in the middle and spreads to both sides, the bars grow
// symmetrically from the center line like a waveform.
Item {
    id: root

    // provider of the values, AudioSpectrum by default (overridable for tests)
    property var spectrum: AudioSpectrum

    property color color: "white"
    property color glowColor: color

    // how many spectrum bands are shown on each side (highest bands are rarely busy)
    property int bands: 36

    property bool mirrored: true

    // falling peak markers, best kept for large visualizers
    property bool showPeaks: true

    property real spacingRatio: 0.45

    readonly property int barCount: mirrored ? bands * 2 : bands
    readonly property real step: width / Math.max(1, barCount)
    readonly property real barWidth: Math.max(1, step * (1.0 - spacingRatio))

    implicitWidth: VLCStyle.dp(440, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(64, VLCStyle.scale)

    Accessible.role: Accessible.Animation
    Accessible.name: qsTr("Audio visualizer")

    Repeater {
        model: root.barCount

        delegate: Item {
            id: bar

            required property int index

            readonly property int band: {
                if (!root.mirrored)
                    return index
                return (index < root.bands) ? (root.bands - 1 - index) : (index - root.bands)
            }

            // reading `frame` makes the binding refresh on every new visual frame
            readonly property real value: (root.spectrum.frame, root.spectrum.band(band))
            readonly property real peakValue: (root.spectrum.frame, root.spectrum.peak(band))

            x: index * root.step + (root.step - root.barWidth) / 2
            width: root.barWidth
            height: root.height

            // soft glow behind loud bars
            Rectangle {
                anchors.centerIn: body
                width: body.width * 2.6
                height: body.height + body.width * 1.6
                radius: width / 2
                color: root.glowColor
                opacity: bar.value * bar.value * 0.28
                visible: opacity > 0.01
            }

            Rectangle {
                id: body

                anchors.centerIn: parent
                width: parent.width
                height: Math.max(width, bar.value * parent.height)
                radius: width / 2

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.alpha(root.color, 0.95) }
                    GradientStop { position: 0.5; color: root.color }
                    GradientStop { position: 1.0; color: Qt.alpha(root.color, 0.95) }
                }

                opacity: 0.3 + 0.7 * Math.min(1.0, bar.value * 1.4)
            }

            // falling peak markers, above and below
            Repeater {
                model: root.showPeaks ? 2 : 0

                Rectangle {
                    required property int index

                    width: bar.width
                    height: width
                    radius: width / 2
                    x: 0
                    y: (bar.height / 2) + (index === 0 ? -1 : 1) * (bar.peakValue * bar.height / 2 + width)
                       - height / 2

                    color: root.color
                    opacity: (bar.peakValue > 0.12 && bar.peakValue > bar.value + 0.06) ? 0.45 * bar.peakValue : 0.0
                    visible: opacity > 0.02
                }
            }
        }
    }
}

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

// Astra Media "aurora": slowly drifting, rotating copies of the artwork.
// Nothing is drawn directly; blur `textureProvider` to get a living,
// colorful background built from the colors of the cover, similar to
// what Apple Music does behind its full screen player.
Item {
    id: root

    property url source

    // 0..1, typically the bass level of the music: blobs swell with it
    property real energy: 0.0

    property bool running: true

    // resolution of the generated texture, it is going to be heavily
    // blurred so it can stay small
    property int resolution: 128

    readonly property Item textureProvider: layerSource

    implicitWidth: resolution
    implicitHeight: resolution

    Item {
        id: canvas

        width: root.resolution
        height: root.resolution

        // static base so that the rotating blobs never reveal empty corners
        Image {
            anchors.centerIn: parent
            width: parent.width * 1.5
            height: width

            source: root.source
            sourceSize: Qt.size(root.resolution, root.resolution)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
        }

        Repeater {
            // position (relative), size factor, rotation period (ms), direction, energy response
            model: [
                { px: 0.22, py: 0.25, size: 1.05, period: 31000, dir:  1, punch: 0.18 },
                { px: 0.80, py: 0.30, size: 0.90, period: 43000, dir: -1, punch: 0.10 },
                { px: 0.30, py: 0.82, size: 0.95, period: 53000, dir: -1, punch: 0.14 },
                { px: 0.78, py: 0.78, size: 1.10, period: 37000, dir:  1, punch: 0.22 }
            ]

            delegate: Image {
                id: blob

                required property var modelData
                required property int index

                width: canvas.width * modelData.size * (1.0 + root.energy * modelData.punch)
                height: width

                x: canvas.width * modelData.px - width / 2 + drift.x
                y: canvas.height * modelData.py - height / 2 + drift.y

                // each blob shows a different part of the artwork
                sourceClipRect: Qt.rect((index % 2) * root.resolution / 4,
                                        Math.floor(index / 2) * root.resolution / 4,
                                        root.resolution * 3 / 4,
                                        root.resolution * 3 / 4)

                source: root.source
                sourceSize: Qt.size(root.resolution, root.resolution)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                smooth: true

                QtObject {
                    id: drift
                    property real x: 0
                    property real y: 0
                }

                RotationAnimation on rotation {
                    from: 0
                    to: 360 * blob.modelData.dir
                    duration: blob.modelData.period
                    loops: Animation.Infinite
                    running: root.running
                    direction: RotationAnimation.Numerical
                }

                SequentialAnimation {
                    loops: Animation.Infinite
                    running: root.running

                    NumberAnimation {
                        target: drift; property: "x"
                        to: canvas.width * 0.12 * blob.modelData.dir
                        duration: blob.modelData.period / 3
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        target: drift; property: "x"
                        to: -canvas.width * 0.12 * blob.modelData.dir
                        duration: blob.modelData.period / 3
                        easing.type: Easing.InOutSine
                    }
                }

                SequentialAnimation {
                    loops: Animation.Infinite
                    running: root.running

                    NumberAnimation {
                        target: drift; property: "y"
                        to: -canvas.height * 0.10
                        duration: blob.modelData.period / 4
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        target: drift; property: "y"
                        to: canvas.height * 0.10
                        duration: blob.modelData.period / 4
                        easing.type: Easing.InOutSine
                    }
                }
            }
        }
    }

    ShaderEffectSource {
        id: layerSource

        width: canvas.width
        height: canvas.height

        // only used as a texture provider for the blur
        visible: false

        sourceItem: canvas
        hideSource: true
        live: root.running
        smooth: true
        textureSize: Qt.size(root.resolution, root.resolution)
    }
}

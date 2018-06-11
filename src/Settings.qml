/*
 * settings.qml
 * Copyright (C) Damien Caliste 2014 <dcaliste@free.fr>
 *
 * settings.qml is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

SilicaFlickable {
    id: settings

    contentHeight: title.height + widgets.childrenRect.height
    contentWidth: parent.width

    PageHeader {
        id: title
        title: qsTr("Settings")
    }

    Column {
        id: widgets
        spacing: Theme.paddingMedium
        width: parent.width - 2 * Theme.horizontalPageMargin
        anchors.top: title.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        TextSwitch {
	        text: qsTr("Draw map double pixel")
            description: qsTr("The tiles of the zoom level above are " +
            "used instead of the normal ones. The rendering appears as if zoomed.")
	    checked: map.double_pixel
            automaticCheck: false
	    onClicked: { map.double_pixel = !map.double_pixel }

            width: parent.width
        }

        ComboBox {
            label: qsTr("Compass mode")
            width: page.width
            description: qsTr("Either point towards physical north pole, or show " +
                         "device orientation relative to map. When switched " +
                         "off, the sensor is completely disabled.")

            currentIndex: map.compass_mode
            onCurrentIndexChanged: map.compass_mode = currentIndex

            menu: ContextMenu {
                MenuItem { text: qsTr("Off") }
                MenuItem { text: qsTr("Point north") }
                MenuItem { text: qsTr("Device orientation") }
            }
        }

        Item {
            width: parent.width
            height: childrenRect.height
            Slider {
                id: refresh
                width: parent.width

                minimumValue: 0.
                maximumValue: 10.
                stepSize: 1.
                label: qsTr("GPS refresh rate")
                value: map.gps_refresh_rate / 1000.
                valueText: (value > 0) ? qsTr("%1 s").arg(value) : qsTr("no update")
                onValueChanged: { map.gps_refresh_rate = value * 1000 }
            }
            Label {
                anchors.top: refresh.bottom
                width: parent.width - 3 * Theme.paddingLarge

                text: qsTr("Minimum time between two GPS updates." +
                " If set to 0, there will be no GPS update.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignJustify
	        anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        
        Item {
            width: parent.width
            height: childrenRect.height
            Slider {
                id: autosave
                width: parent.width

                minimumValue: 0.
                maximumValue: 15.
                stepSize: 1.
                label: qsTr("Track autosave rate")
                value: map.track_autosave_rate / 60.
                valueText: (value > 0) ? qsTr("%1 min.").arg(value) : qsTr("on demand")
                onValueChanged: { map.track_autosave_rate = value * 60 }
            }
            Label {
                anchors.top: autosave.bottom
                width: parent.width - 3 * Theme.paddingLarge

                text: qsTr("Time between two automatic track dumps on disk." +
                " Autosaving a track without providing a filename" +
                " will result to the track being saved in" +
                " $HOME/.local/share/harbour-maep-qt/track.gpx")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignJustify
	        anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        
        Item {
            width: parent.width
            height: childrenRect.height
            Slider {
                id: metric
                width: parent.width

                minimumValue: 0.
                maximumValue: 100.
                stepSize: 10.
                label: qsTr("Track metric accuracy")
                value: map.track_metric_accuracy
                valueText: (value > 0) ? qsTr("%1 m").arg(value) : qsTr("keep all")
                onValueChanged: { map.track_metric_accuracy = value }
            }
            Label {
                anchors.top: metric.bottom
                width: parent.width - 3 * Theme.paddingLarge

                text: qsTr("Points in track are displayed if horizontal" +
                " accuracy is below this value.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignJustify
	        anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    VerticalScrollDecorator { flickable: settings }
}

/*
 * settings.qml
 * Copyright (C) Damien Caliste 2014 <dcaliste@free.fr>
 *
 * about.qml is free software: you can redistribute it and/or modify it
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

    contentHeight: childrenRect.height
    contentWidth: parent.width

    PageHeader {
        id: title
        title: "Settings"
    }

    Column {
        spacing: Theme.paddingLarge
        width: parent.width
        anchors.top: title.bottom

        TextSwitch {
	    text: "Draw map double pixel"
            description: "The tiles of the zoom level above are" +
            "used instead of the normal ones. The rendering appears as if zoomed."
	    checked: map.double_pixel
            automaticCheck: false
	    onClicked: { map.double_pixel = !map.double_pixel }

            width: parent.width
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
                label: "GPS refresh rate"
                value: map.gps_refresh_rate / 1000.
                valueText: (value > 0) ? value + " s" : "No update"
                onValueChanged: { map.gps_refresh_rate = value * 1000 }
            }
            Label {
                anchors.top: refresh.bottom
                width: parent.width - 6 * Theme.paddingLarge

                text: "Minimum time between two GPS updates." +
                " If set to 0, there will be no GPS update."
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignJustify
	        anchors.horizontalCenter: parent.horizontalCenter
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
                label: "Track autosave rate"
                value: map.track_autosave_rate / 60.
                valueText: (value > 0) ? value + " min." : "On demand"
                onValueChanged: { map.track_autosave_rate = value * 60 }
            }
            Label {
                anchors.top: refresh.bottom
                width: parent.width - 6 * Theme.paddingLarge

                text: "Time between two track dumps on disk." +
                " If set to 0, tracks are saved only on demand." +
                " Autosaving a track without providing a filename" +
                " will result to the track being saved in" +
                " $HOME/.local/share/maep/track.gpx"
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignJustify
	        anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
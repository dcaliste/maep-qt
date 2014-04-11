/*
 * TrackView.qml
 * Copyright (C) Damien Caliste 2014 <dcaliste@free.fr>
 *
 * TrackView.qml is free software: you can redistribute it and/or modify it
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
import Maep 1.0

Column {
    Item {
        width: parent.width
        height: Theme.itemSizeMedium
	Label {
            anchors.right: parent.right
            text: "No track"
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    BackgroundItem {
        id: item_gps
	contentHeight: Theme.itemSizeSmall
        enabled: map.gps_coordinate.latitude <= 90 && map.gps_coordinate.latitude >= -90
        opacity: enabled ? 1.0 : 0.4
        Row {
            anchors.fill: parent
            spacing: Theme.paddingMedium
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/icon-m-gps"
            }
	    Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "capture GPS position"
		color: item_gps.highlighted ? Theme.highlightColor : Theme.primaryColor
	    }
        }
        onClicked: map.track_capture = true
    }
    BackgroundItem {
        id: item_file
	contentHeight: Theme.itemSizeSmall
        opacity: enabled ? 1.0 : 0.4
        Row {
            anchors.fill: parent
            spacing: Theme.paddingMedium
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/icon-m-device-download"
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "import a local file"
		color: item_file.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }
        onClicked: page.importTrack()
    }
    BackgroundItem {
        id: item_osm
	contentHeight: Theme.itemSizeSmall
        enabled: false
        opacity: enabled ? 1.0 : 0.4
        Row {
            anchors.fill: parent
            spacing: Theme.paddingMedium
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/icon-m-cloud-download"
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "import from OSM"
		color: item_osm.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }
    }
}

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

SilicaFlickable {
    property Track track: null

    id: trackitem
    contentHeight: track ? trackdata.height : trackholder.height
    
    Column {
        id: trackdata
        visible: track
        width: parent.width - 2 * Theme.paddingSmall
        spacing: Theme.paddingSmall
        TrackHeader {
            track: trackitem.track
            width: parent.width
        }
        Row {
            spacing: Theme.paddingMedium
            Label {
                width: trackitem.width / 2
                horizontalAlignment: Text.AlignRight
                font.pixelSize: Theme.fontSizeSmall
                text: "length (duration)"
            }
            Label {
                function duration(time) {
                    if (time < 60) {
                        return time + " s"
                    } else if (time < 3600) {
                        var m = Math.floor(time / 60)
                        var s = time - m * 60
                        return  m + " m " + s + " s"
                    } else {
                        var h = Math.floor(time / 3600)
                        var m = Math.floor((time - h * 3600) / 60)
                        var s = time - h * 3600 - m * 60
                        return h + " h " + m + " m " + s + " s"
                    }
                }
                width: trackitem.width / 2
                horizontalAlignment: Text.AlignLeft
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
		text: if (track) { (track.length >= 1000) ? (track.length / 1000).toFixed(1) + " km (" + duration(track.duration) + ")" : track.length.toFixed(0) + " m (" + duration(track.duration) + ")"} else ""
            }
        }
        Row {
            spacing: Theme.paddingMedium
            Label {
                width: trackitem.width / 2
                horizontalAlignment: Text.AlignRight
                font.pixelSize: Theme.fontSizeSmall
                text: "average speed"
            }
            Label {
                width: trackitem.width / 2
                horizontalAlignment: Text.AlignLeft
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
                text: track && track.duration > 0 ? (track.length / track.duration * 3.6).toFixed(2) + " km/h":"not available"
            }
        }
    }
    Column {
        id: trackholder
        visible: !track
        width: parent.width - 2 * Theme.paddingMedium
	Label {
            anchors.right: parent.right
            text: "No track"
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
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
    VerticalScrollDecorator { flickable: trackitem }
}

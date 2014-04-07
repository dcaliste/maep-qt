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
    property Track track: null
    property bool tracking: false
    property alias wptFocus: wpt_desc.focus

    id: trackdata
    visible: track
    width: parent.width - 2 * Theme.paddingSmall
    spacing: Theme.paddingSmall
    Row {
        spacing: Theme.paddingMedium
        Label {
            width: trackdata.width / 2
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
            width: trackdata.width / 2
            horizontalAlignment: Text.AlignLeft
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.highlightColor
	    text: if (track) { (track.length >= 1000) ? (track.length / 1000).toFixed(1) + " km (" + duration(track.duration) + ")" : track.length.toFixed(0) + " m (" + duration(track.duration) + ")"} else ""
        }
    }
    Row {
        spacing: Theme.paddingMedium
        Label {
            width: trackdata.width / 2
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Theme.fontSizeSmall
            text: "average speed"
        }
        Label {
            width: trackdata.width / 2
            horizontalAlignment: Text.AlignLeft
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.highlightColor
            text: track && track.duration > 0 ? (track.length / track.duration * 3.6).toFixed(2) + " km/h":"not available"
        }
    }
    Row {
        enabled: tracking
        opacity: enabled ? 1.0 : 0.4
        spacing: Theme.paddingSmall
        width: parent.width
        IconButton {
            id: wpt_prev
            anchors.verticalCenter: wpt_desc.verticalCenter
	    icon.source: "image://theme/icon-m-previous"
        }
        TextField {
            id: wpt_desc
            width: parent.width - wpt_prev.width - wpt_next.width - 2 * parent.spacing
            height: Theme.itemSizeMedium
            placeholderText: "new waypoint description"
            label: "track has xx way points"
        }
        IconButton {
            id: wpt_next
            anchors.verticalCenter: wpt_desc.verticalCenter
	    icon.source: "image://theme/icon-m-add"
        }
    }    
}

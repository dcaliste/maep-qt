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

Row {
    property Track track: null
    
    height: track_title.height
    spacing: Theme.paddingSmall

    RemorsePopup { id: remorse }

    Formatter { id: formatter }

    IconButton {
        id: track_clear
        anchors.verticalCenter: track_title.verticalCenter
	icon.source: "image://theme/icon-m-remove"
	onClicked: remorse.execute("Clear current track", map.setTrack)
    }
    Column {
        id: track_title
        width: parent.width - track_save.width - track_clear.width
        Item {
            width: parent.width
            height: basename_lbl.height
            Label {
                id: basename_lbl
	        function basename(url) {
                    return url.substring(url.lastIndexOf("/") + 1)
                }
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
                text: (track)?(track.path.length > 0)?basename(track.path):"unsaved track":""
                truncationMode: TruncationMode.Fade
                width: parent.width - acquisision.width
            }
            Label {
                id: acquisision
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: (track) ? "(" + formatter.formatDate(new Date(track.startDate * 1000), Formatter.TimepointRelative) + ")":""
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }
        }
        Label {
	    function dirname(url) {
                return url.substring(0, url.lastIndexOf("/"))
            }
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: (track) ? (track.path.length > 0) ? "in " + dirname(track.path):"save with the icon beside":""
            horizontalAlignment: Text.AlignRight
            truncationMode: TruncationMode.Fade
            width: parent.width - Theme.paddingMedium
            anchors.right: parent.right
        }
    }
    IconButton {
        id: track_save
        anchors.verticalCenter: track_title.verticalCenter
	icon.source: (track && track.path.length > 0) ? "image://theme/icon-m-sync" : "image://theme/icon-m-device-upload"
	onClicked: if (track && track.path.length == 0) {
            pageStack.push(tracksave, { track: track })
        } else {
            track.toFile(track.path)
        }
    }
}

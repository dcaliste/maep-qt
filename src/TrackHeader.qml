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

Item {
    id: root
    signal clicked()
    
    property Track track: null
    property bool detailVisible: false

    height: (track) ? track_main.height : track_holder.height

    Item {
        id: track_main
        visible: track
        height: Theme.itemSizeMedium
        width: parent.width

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.paddingSmall

            RemorsePopup { id: remorse }

            Formatter { id: formatter }

            IconButton {
                id: track_clear
                visible: track
                anchors.verticalCenter: track_button.verticalCenter
	        icon.source: "image://theme/icon-m-remove"
	        onClicked: remorse.execute("Clear current track", map.setTrack)
            }
            BackgroundItem {
                id: track_button
                width: track_title.width
                contentHeight: track_title.height
                onClicked: root.clicked()
                Column {
                    id: track_title
                    width: root.width - track_save.width - track_clear.width - 2 * Theme.paddingSmall
                    Label {
	                function basename(url) {
                            return url.substring(url.lastIndexOf("/") + 1)
                        }
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        text: (track)?(track.path.length > 0)?basename(track.path):"Unsaved track":""
                        horizontalAlignment: (track) ? Text.AlignLeft : Text.AlignRight
                        truncationMode: TruncationMode.Fade
                        width: parent.width
                    }
                    Label {
	                function location(url, date) {
                            if (url.length > 0) {
                                return url.substring(0, url.lastIndexOf("/")) + " (" + formatter.formatDate(date, Formatter.TimepointRelative) + ")"
                            } else {
                                return "export to a local file"
                            }
                        }
                        visible: track
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        text: (track) ? location(track.path, new Date(track.startDate * 1000)) : ""
                        horizontalAlignment: Text.AlignRight
                        truncationMode: TruncationMode.Fade
                        width: parent.width - Theme.paddingMedium
                        anchors.right: parent.right
                    }
                }
                Image {
                    z: track_title.z - 1
                    anchors.centerIn: parent
                    source: root.detailVisible ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
                    opacity: track_button.highlight ? 1.0 : 0.5
                }
            }
            IconButton {
                id: track_save
                visible: track
                anchors.verticalCenter: track_button.verticalCenter
	        icon.source: (track && track.path.length > 0) ? "image://theme/icon-m-sync" : "image://theme/icon-m-device-upload"
	        onClicked: if (track && track.path.length == 0) {
                    pageStack.push(tracksave, { track: track })
                } else {
                    track.toFile(track.path)
                }
            }
        }
    }

   Column {
        id: track_holder
        visible: !track
        width: parent.width - 2 * Theme.paddingMedium
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
}
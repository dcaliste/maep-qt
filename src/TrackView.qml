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
    id: root

    property Track track: null
    property variant currentPlace
    property bool tracking: false
    property bool wptFocus: false
    property bool detailVisible: false

    ListModel {
        id: waypoints
        function refresh(track, edit) {
            waypoints.clear();
            if (!track) { return }
            for (var i = 0; i < track.getWayPointLength(); i++) {
                var wpt = Object();
                wpt.index = i
                wpt.name = track.getWayPoint(i, Track.FIELD_NAME);
                wpt.comment = track.getWayPoint(i, Track.FIELD_COMMENT);
                wpt.description = track.getWayPoint(i, Track.FIELD_DESCRIPTION);
                waypoints.append(wpt);
            }
            if (edit) { editable(true) }
        }
        function editable(value) {
            // If true, append an empty wpt to the model.
            if (value) {
                var wpt = Object();
                wpt.index = waypoints.count
                wpt.name = ""
                wpt.comment = ""
                wpt.description = ""
                waypoints.append(wpt);
            } else {
                waypoints.remove(waypoints.count - 1)
            }
        }
    }
    onTrackChanged: waypoints.refresh(track, tracking)
    onTrackingChanged: waypoints.editable(tracking)

    width: parent.width - 2 * Theme.paddingSmall
    spacing: Theme.paddingSmall

    Formatter { id: formatter }

    Item {
        width: parent.width
        height: track_button.height
        visible: !wptFocus
        ListItem {
            id: track_button
            width: parent.width
            contentHeight: Theme.itemSizeMedium
            onClicked: root.detailVisible = !root.detailVisible
            
            RemorseItem { id: remorse }

            menu: ContextMenu {
                MenuItem {
                    text: "clear"
                    onClicked: {
                        root.detailVisible = false
                        remorse.execute(track_button,
                                        "Clear current track", map.setTrack)
                    }
                }
                MenuItem {
                    text: "save on device"
                    onClicked: if (track && track.path.length == 0) {
                        pageStack.push(tracksave, { track: track })
                    } else {
                        track.toFile(track.path)
                    }
                }
                MenuItem {
                    text: "export to OSM"
                    enabled: false
                }
            }
            PageHeader {
	        function basename(url) {
                    return url.substring(url.lastIndexOf("/") + 1)
                }
                title: (track)?(track.path.length > 0)?basename(track.path):"Unsaved track":""
                height: Theme.itemSizeMedium
            }
            Label {
                function details(tr) {
                    if (!tr || tr.duration == 0.) { return "" }
                    var dist = ""
                    if (tr.length >= 1000) {
                        dist = (tr.length / 1000).toFixed(1) + " km"
                    } else {
                        dist = tr.length.toFixed(0) + " m"
                    }
                    var time = tr.duration
                    var duration = ""
                    if (time < 60) {
                        duration = time + " s"
                    } else if (time < 3600) {
                        var m = Math.floor(time / 60)
                        var s = time - m * 60
                        duration = m + " m " + s + " s"
                    } else {
                        var h = Math.floor(time / 3600)
                        var m = Math.floor((time - h * 3600) / 60)
                        duration = h + " h " + m + " m "
                    }
                    return dist + " (" + duration + ") - " + (tr.length / time * 3.6).toFixed(2) + " km/h"
                }
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: details(track)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
            }
            Image {
                anchors.left: parent.left
                source: root.detailVisible ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.paddingMedium
            }
        }
    }
    Label {
	function location(url, date) {
            return "in " + url.substring(0, url.lastIndexOf("/")) + " (" + formatter.formatDate(date, Formatter.TimepointRelative) + ")"
        }
        visible: root.detailVisible && !wptFocus && track && track.path.length > 0
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
        text: (track) ? location(track.path, new Date(track.startDate * 1000)) : ""
        horizontalAlignment: Text.AlignRight
        truncationMode: TruncationMode.Fade
        width: parent.width - Theme.paddingMedium
        anchors.right: parent.right
    }
    Item {
        visible: root.detailVisible && waypoints.count > 0
        width: parent.width
        height: wptview.height
        clip: true
        Image {
            visible: waypoints.count > 1
            source: "image://theme/icon-m-previous"
            x: - width / 2
            anchors.verticalCenter: parent.verticalCenter
            z: wptview.z + 1
        }
        SlideshowView {
            id: wptview
            width: parent.width - 2 * Theme.paddingLarge
            anchors.horizontalCenter: parent.horizontalCenter
            height: Theme.itemSizeMedium
            itemWidth: width
            model: waypoints
            onCurrentIndexChanged: track.highlightWayPoint(currentIndex)

            delegate: TextField {
                enabled: wptview.currentIndex == model.index
                opacity: enabled ? 1.0 : 0.4
                width: wptview.width
                placeholderText: "new waypoint description"
                label: (tracking && (model.index == waypoints.count - 1)) ? "new waypoint at GPS position" : "name of waypoint " + (model.index + 1)
                text: model.name
	        EnterKey.text: (tracking && model.index == waypoints.count - 1) ? "add" : "update"
	        EnterKey.onClicked: {
                    if (tracking && model.index == waypoints.count - 1) {
                        track.addWayPoint(currentPlace, text, "", "")
                        track.highlightWayPoint(model.index);
                        waypoints.editable(true)
                    } else {
                        track.setWayPoint(model.index, Track.FIELD_NAME, text)
	            }
                    map.focus = true
                }
                onActiveFocusChanged: wptFocus = activeFocus
            }

        }
        Image {
            visible: waypoints.count > 1
            source: "image://theme/icon-m-next"
            x: parent.width - width / 2
            anchors.verticalCenter: parent.verticalCenter
            z: wptview.z + 1
        }
    }
}

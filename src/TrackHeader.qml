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
import QtPositioning 5.2
import harbour.maep.qt 1.0

Column {
    id: root
    property Track track: null
    property bool deletePending: remorse.pending

    function deleteTrack() {
        remorse.execute(remorseHook,
                        "Clear current track", map.setTrack)
    }

    PageHeader {
        height: Theme.itemSizeMedium
        title: root.track ? "Track" : "No track"
    }
    BackgroundItem {
        id: item_current
        contentHeight: Theme.itemSizeSmall
        visible: root.track
        Item {
            id: remorseHook
            anchors.fill: parent
            RemorseItem { id: remorse }
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
        onClicked: {
            remorse.trigger()
            map.track_capture = true
        }
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
        onClicked: {
            remorse.trigger()
            page.importTrack()
        }
    }
    BackgroundItem {
        id: item_backup
        contentHeight: Theme.itemSizeSmall
        opacity: enabled ? 1.0 : 0.4
        Row {
            anchors.fill: parent
            spacing: Theme.paddingMedium
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/icon-m-backup"
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "last autosaved track"
                color: item_backup.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }
        onClicked: {
            remorse.trigger()
            map.track_capture = false
            var track = defaultTrack.createObject(map)
            if (track.setFromBackup()) map.setTrack(track)
        }
        Component {
            id: defaultTrack
            Track { }
        }
    }
    /*BackgroundItem {
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
    }*/
}

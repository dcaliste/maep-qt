/*
 * header.qml
 * Copyright (C) Damien Caliste 2014 <dcaliste@free.fr>
 *
 * header.qml is free software: you can redistribute it and/or modify it
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
import harbour.maep.qt 1.0

SilicaFlickable {
    id: root

    property alias searchFocus: placeHeader.searchFocus
    property alias searchText: placeHeader.text

    function searchResults(lst) {
        placeHeader.searchResults(lst)
    }
    
    property Conf conf:  Conf {  }
    property int currentIndex: {
        var action = conf.getString("ui_start_action", "search")
        var id = 0
        if (action == "track") {
            id = 1
        }
        setAction(id)
        return id
    }
    function setAction(id) {
        if (id == 1) {
            contentX = root.width
            contentHeight = Qt.binding(function() { return trackView.height })
            height = Qt.binding(function() { return Math.min(trackView.height,
                5 * Theme.itemSizeSmall + Theme.itemSizeMedium) })
        } else {
            contentX = 0
            contentHeight = Qt.binding(function() { return placeHeader.height })
            height = Qt.binding(function() { return Math.min(placeHeader.height,
                5 * Theme.itemSizeSmall + Theme.itemSizeMedium) })
        } 
    }
    onCurrentIndexChanged: setAction(currentIndex)

    Component.onDestruction: if (currentIndex == 0) {
        conf.setString("ui_start_action", "search")
    } else if (currentIndex == 1) {
        conf.setString("ui_start_action", "track")
    }
    
    height: Theme.itemSizeMedium
    contentWidth: content.width

    Behavior on height {
        enabled: !track_details.menu
        NumberAnimation { easing.type: Easing.InOutCubic }
    }

    Behavior on contentX {
        NumberAnimation { easing.type: Easing.InOutCubic }
    }

    Row {
        id: content

        PlaceHeader {
            id: placeHeader
            width: root.width
            currentPlace: map.coordinate
            onSearchRequest: {
                map.focus = true
                map.setSearchRequest(text)
            }
            onSelection: map.setLookAt(lat, lon)
        }
        Column {
            id: trackView
            width: root.width
            TrackHeader {
                visible: !map.track
                width: parent.width
            }
            TrackView {
                id: track_details
                width: parent.width
                visible: track
                track: map.track
                tracking: map.track_capture
                color: map.trackColor
                lineWidth: map.trackWidth
                currentPlace: map.gps_coordinate
                onWptMovingChanged: root.flickableDirection = (wptMoving) ? Flickable.VerticalFlick : Flickable.HorizontalAndVerticalFlick
                onRequestColor: map.trackColor = color
                onRequestWidth: map.trackWidth = width
            }
        }
    }
    
    onMovingHorizontallyChanged: if (movingHorizontally) { flickableDirection = Flickable.HorizontalFlick }
    onMovingVerticallyChanged: if (movingVertically) { flickableDirection = Flickable.VerticalFlick }
    onMovementEnded: {
        if (contentX < root.width / 2) {
            contentX = 0
            currentIndex = 0
        } else {
            contentX = root.width
            currentIndex = 1
        }
        track_details.wptMoving = false
        flickableDirection = Flickable.HorizontalAndVerticalFlick
    }

    VerticalScrollDecorator { flickable: root
                              visible: root.contentHeight > root.height }
}

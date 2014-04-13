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

SilicaFlickable {
    id: root
    property alias trackFocus: track_details.wptFocus

    property alias searchFocus: placeHeader.searchFocus
    property alias searchText: placeHeader.text

    function searchResults(lst) {
        placeHeader.searchResults(lst)
    }
    
    // Tell SilicaFlickable the height of its content.
    height: Math.min(content.height, content.maxHeight[content.currentIndex])
    contentHeight: content.height
    contentWidth: content.width
    //clip: true

    Behavior on height {
        NumberAnimation { easing.type: Easing.InOutCubic }
    }
    Behavior on contentX {
        NumberAnimation { easing.type: Easing.InOutCubic }
    }

    Row {
        id: content
        height: currentItem.height
        property int currentIndex: 0
        property Item currentItem: placeHeader
        property variant maxHeight: [5 * Theme.itemSizeSmall + Theme.itemSizeMedium,
                                     3 * Theme.itemSizeSmall + Theme.itemSizeMedium]
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
                currentPlace: map.gps_coordinate
            }
        }
    }
    onMovingHorizontallyChanged: if (movingHorizontally) { flickableDirection = Flickable.HorizontalFlick }
    onMovingVerticallyChanged: if (movingVertically) { flickableDirection = Flickable.VerticalFlick }
    onMovementEnded: {
        if (contentX < root.width / 2) {
            contentX = 0
            content.currentIndex = 0
            content.currentItem = placeHeader
        } else {
            contentX = root.width
            content.currentIndex = 1
            content.currentItem = trackView
        }
        flickableDirection = Flickable.HorizontalAndVerticalFlick
    }

    VerticalScrollDecorator { flickable: root
                              visible: root.contentHeight > root.height }
}
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
    property int action: 0
    function nextAction() {
        action = (action + 1) % 2
    }
    function prevAction() {
        action = (action - 1) % 2
    }
    property alias trackFocus: track_details.wptFocus

    property alias searchFocus: placeHeader.searchFocus
    property alias searchText: placeHeader.text
    property alias resultVisible: placeHeader.resultVisible
    signal searchRequest(string text)
    signal showResults(bool status)
    function searchResults(lst) {
        placeHeader.searchResults(lst)
    }
    
    // Tell SilicaFlickable the height of its content.
    height: Math.min(content.height, 3 * Theme.itemSizeSmall + Theme.itemSizeMedium)
    contentHeight: content.height
    //clip: true

    Behavior on height {
        NumberAnimation { easing.type: Easing.InOutCubic }
    }

    Column {
        id: content
        width: parent.width
        PlaceHeader {
            id: placeHeader
            visible: action == 0
            width: parent.width
            height: Theme.itemSizeMedium
            onSearchRequest: root.searchRequest(text)
            onShowResults: root.showResults(status)
        }
        Column {
            id: action1
            visible: action == 1
            width: parent.width
            TrackHeader {
                id: trackHolder
                visible: !map.track
                width: parent.width
            }
            TrackView {
                id: track_details
                width: parent.width
                visible: track
                track: map.track
                tracking: map.track_capture
                currentPlace: map.coordinate
            }
        }
    }

    VerticalScrollDecorator { flickable: root; visible: root.contentHeight > root.height }
}
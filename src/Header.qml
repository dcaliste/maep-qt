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
    property alias searchFocus: search.focus
    property alias trackFocus: track_details.wptFocus
    property alias searchText: search.text
    property bool resultVisible: false
    signal searchRequest(string text)
    function searchResults(lst) {
        search.label = lst.length + " place(s) found"
	busy.visible = false
        search_icon.visible = (lst.length > 0)
    }
    signal showResults(bool status)
    
    // Tell SilicaFlickable the height of its content.
    height: Math.min(content.height, 3 * Theme.itemSizeSmall + Theme.itemSizeMedium)
    contentHeight: content.height

    Column {
        id: content
        width: parent.width
        Item {
            id: action0
            visible: action == 0
            width: parent.width
            height: Theme.itemSizeMedium
            TextField {
                id: search
                width: parent.width - maep.width - ((search_icon.visible || busy.visible)?search_icon.width:0)
                placeholderText: "Enter a place name"
	        label: "Place search"
	        anchors.verticalCenter: parent.verticalCenter
	        EnterKey.text: "search"
	        EnterKey.onClicked: {
                    search_icon.visible = false
                    busy.visible = true
                    label = "Searching…"
                    root.searchRequest(text)
	        }
	        onFocusChanged: { if (focus) { selectAll() } }
            }
            Item {
                anchors.right: maep.left
                height: parent.height
                width: search_icon.width
	        BusyIndicator {
	   	    id: busy
                    visible: false
                    running: visible
                    size: BusyIndicatorSize.Small
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                IconButton {
                    id: search_icon
                    icon.source: root.resultVisible ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
                    visible: false
                    onClicked: root.showResults(root.resultVisible)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            PageHeader {
                id: maep
                width: 130
                title: "Mæp"
                anchors.right: parent.right
            }
        }
        Column {
            id: action1
            visible: action == 1
            width: parent.width
            TrackHeader {
                id: trackHolder
                visible: !map.track
                width: parent.width - 2 * Theme.paddingMedium
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

/*    PushUpMenu {
        visible: action == 1 && track_details.visible
        MenuItem {
            text: "Upload track to OSM"
            enabled: false
            //onClicked: pageStack.push(tracksave, { track: map.track })
        }
        MenuItem {
            text: "Import track"
            onClicked: page.importTrack()
        }
        MenuItem {
            text: "Export track"
            onClicked: pageStack.push(tracksave, { track: map.track })
        }
	onActiveChanged: { active ? map.opacity = Theme.highlightBackgroundOpacity : map.opacity = 1 }
    }*/

    VerticalScrollDecorator { flickable: root; visible: root.contentHeight > root.height }
}
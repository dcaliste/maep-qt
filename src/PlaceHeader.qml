/*
 * PlaceHeader.qml
 * Copyright (C) Damien Caliste 2014 <dcaliste@free.fr>
 *
 * PlaceHeader.qml is free software: you can redistribute it and/or modify it
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

Item {
    property alias text: search.text
    property alias searchFocus: search.focus
    property alias searchText: search.text
    property bool resultVisible: false

    signal searchRequest(string text)
    signal showResults(bool status)

    function searchResults(lst) {
        search.label = lst.length + " place(s) found"
	busy.visible = false
        search_icon.visible = (lst.length > 0)
    }

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
            searchRequest(text)
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
            icon.source: resultVisible ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
            visible: false
            onClicked: showResults(!resultVisible)
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    PageHeader {
        id: maep
        width: 130
        height: Theme.itemSizeMedium
        title: "Mæp"
        anchors.right: parent.right
    }
}

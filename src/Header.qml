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
    property alias searchFocus: search.focus
    property alias searchLabel: search.label
    property alias searchText: search.text
    property alias busy: busy.visible
    
    // Tell SilicaFlickable the height of its content.
    height: Theme.itemSizeMedium //childrenRect.height
    contentHeight: childrenRect.height

    Item {
        width: parent.width
        height: Theme.itemSizeMedium
        TextField {
            id: search
            width: parent.width - maep.width - ((search_icon.visible || busy.visible)?search_icon.width:0)
            height: parent.height
            placeholderText: "Enter a place name"
	    label: "Place search"
	    anchors.verticalCenter: parent.verticalCenter
	    EnterKey.text: "search"
	    EnterKey.onClicked: {
                drawer.disable(placelist)
		//placeview.model = null
                search_icon.visible = false
                busy.visible = true
		map.focus = true
                map.setSearchRequest(text)
	    }
	    onFocusChanged: { if (focus) { selectAll(); drawer.disable(trackview) } }
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
                property bool opened: drawer.open && drawer_background.sourceComponent == placelist
                icon.source: opened ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
                visible: drawer_background.sourceComponent == placelist
                onClicked: opened ? drawer.disable(placelist):drawer.enable(placelist)
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
}

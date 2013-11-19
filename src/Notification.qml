/*
 * Notification.qml
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
 *
 * Notification.qml is free software: you can redistribute it and/or modify it
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
    id: banner
    property alias text: banner_label.text
    property bool active: false

    Rectangle {
	id: banner_rect
	color: Theme.highlightColor
	anchors.left: parent.left
	anchors.right: parent.right
	height: childrenRect.height
	y: -height
	opacity: 0
	z: 10
    	Label {
	    id: banner_label
	    anchors.centerIn: parent
	    color: Theme.primaryColor
	    font.pixelSize: Theme.fontSizeExtraSmall
	}
	states: [
	    State {
                name: "visible"
                when: banner.active
	        PropertyChanges { target: banner_rect; y: Theme.itemSizeExtraSmall; opacity: 1 }
	    },
	    State {
	        name: "hidden"
	        when: !banner.active
	        PropertyChanges { target: banner_rect; y: -height; opacity: 0 }
	    } ]
	transitions: Transition {
	    PropertyAnimation { property: "y"; duration: 1000 }
	    PropertyAnimation { property: "opacity"; duration: 1000 }
	}
    }
    Timer {
        id: timer
        interval: 5000
        repeat: false
	running: active
        onTriggered: { active = false }
    }
}

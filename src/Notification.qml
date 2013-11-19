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

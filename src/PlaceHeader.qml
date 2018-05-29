/*
 * PlaceHeader.qml
 * Copyright (C) Damien Caliste 2014-2018 <dcaliste@free.fr>
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

Column {
    property alias text: search.text
    property alias searchFocus: search.focus
    property alias searchText: search.text
    property alias resultVisible: resultList.visible
    property alias resultModel: resultList.model
    property alias searching: busy.running
    property variant currentPlace

    signal searchRequest(string text)
    signal selection(string place, real lat, real lon)

    function searchResults(lst) {
        searching = false
        resultList.model = lst
    }

    PageHeader {
        id: pageHeader
        width: parent.width
        height: Theme.itemSizeMedium
        title: "Mæp"
        TextField {
            id: search
            parent: pageHeader.extraContent
            width: parent.width
                - (search_icon.visible ? search_icon.width : 0)
                - (busy.visible ? busy.width : 0)
            textLeftMargin: 0
            placeholderText: "Enter a place name"
            label: busy.visible ? "Searching…"
                : resultVisible
                    ? resultList.model.length + " place(s) found" : "Place search"
            anchors.verticalCenter: parent.verticalCenter
            EnterKey.text: "search"
            EnterKey.onClicked: {
                searching = true
                resultList.model = undefined
                searchRequest(text)
	        }
            onFocusChanged: { if (focus) { selectAll() } }
        }
        BusyIndicator {
	        id: busy
            parent: pageHeader.extraContent
            anchors.right: parent.right
            anchors.rightMargin: pageHeader.page.isLandscape ? Theme.paddingLarge : Theme.paddingSmall
            visible: running
            size: BusyIndicatorSize.Small
            anchors.verticalCenter: parent.verticalCenter
        }
        IconButton {
            id: search_icon
            visible: resultVisible && !busy.visible
            parent: pageHeader.extraContent
            anchors.right: parent.right
            anchors.rightMargin: pageHeader.page.isLandscape ? Theme.paddingLarge : Theme.paddingSmall
            icon.source: resultList.expanded ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
            onClicked: resultList.expanded = !resultList.expanded
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    Repeater {
        id: resultList
        property bool expanded: true
        visible: model !== undefined && model.length > 0
        
        ListItem {
            visible: resultList.expanded
	        contentHeight: Theme.itemSizeSmall
            Image {
                id: img_go
                source: "image://theme/icon-m-right"
                anchors.right: parent.right
                anchors.leftMargin: Theme.paddingSmall
                anchors.rightMargin: Theme.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
            }
	        Label {
                text: model.name
	            font.pixelSize: Theme.fontSizeSmall
                truncationMode: TruncationMode.Fade
                anchors.leftMargin: Theme.paddingSmall
                anchors.left: parent.left
                anchors.right: img_go.left
                anchors.top: parent.top
                anchors.topMargin: Theme.paddingMedium
	            color: highlighted ? Theme.highlightColor : Theme.primaryColor
	        }
	        Label {
                text: model.country
	            font.pixelSize: Theme.fontSizeExtraSmall
                anchors.leftMargin: Theme.paddingLarge
                anchors.bottom: parent.bottom
                anchors.left: parent.left
	            color: highlighted ? Theme.highlightColor : Theme.secondaryColor
	        }
	        Label {
	            property real dist: currentPlace.distanceTo(model.coordinate)
	            font.pixelSize: Theme.fontSizeExtraSmall
	            text: dist >= 1000 ? "at " + (dist / 1000).toFixed(1) + " km" : "at " + dist.toFixed(0) + " m"
	            color: Theme.secondaryColor
	            anchors.right: img_go.left
	            anchors.bottom: parent.bottom
	        }
	        onClicked: {
                resultList.expanded = false
                search.text = model.name
                selection(model.name, model.coordinate.latitude,
                          model.coordinate.longitude)
            }
        }
    }
}

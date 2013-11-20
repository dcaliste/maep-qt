/*
 * about.qml
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
 *
 * about.qml is free software: you can redistribute it and/or modify it
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
    id: about
    property string version
    property string date
    property alias authors: lbl_authors.text
    property alias license: lbl_license.text
    property string viewable: "authors"

    PullDownMenu {
        MenuItem {
            text: "Authors"
            onClicked: { viewable = "authors" }
        }
        MenuItem {
            text: "License"
            onClicked: { viewable = "license" }
        }
    }

    contentHeight: childrenRect.height
    contentWidth: childrenRect.width

    PageHeader { id: title; title: "About Mæp" }
    Image {
        id: icon
        source: "file:///usr/share/icons/hicolor/86x86/apps/maep-qt.png"
        anchors.horizontalCenter: parent.horizontalCenter
	anchors.top: title.bottom
    }
    Label {
        id: subtitle
        width: parent.width
        text: "A small and fast tile map"
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeMedium
        horizontalAlignment: Text.AlignHCenter
        anchors.top: icon.bottom
        anchors.bottomMargin: Theme.paddingMedium
    }
    Label {
        id: lbl_version
        width: parent.width
        text: "Version " + version + " - " + date
        color: Theme.primaryColor
        font.pixelSize: Theme.fontSizeSmall
        horizontalAlignment: Text.AlignHCenter
	anchors.topMargin: Theme.paddingMedium
        anchors.top: subtitle.bottom
    }
    Label {
        id: copyright
        width: parent.width
        text: "Copyright 2009-2013"
        color: Theme.primaryColor
        font.pixelSize: Theme.fontSizeSmall
        horizontalAlignment: Text.AlignHCenter
        anchors.top: lbl_version.bottom
        anchors.bottomMargin: Theme.paddingMedium
    }
    Item {
        id: item_authors
        opacity: 0
        anchors.top: copyright.bottom
	width: childrenRect.width
        Label {
	    id: lbl_title_authors
            text: "Authors"
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeMedium
            anchors.topMargin: Theme.paddingSmall
        }
        Label {
            id: lbl_authors
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
	    anchors.top: lbl_title_authors.bottom
        }
    }
    Item {
        id: item_license
        opacity: 0
        anchors.top: copyright.bottom
	width: childrenRect.width
        Label {
	    id: lbl_title_license
            text: "License"
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeMedium
            anchors.topMargin: Theme.paddingSmall
        }
        Label {
            id: lbl_license
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
	    anchors.top: lbl_title_license.bottom
        }
    }
    states: [
	State {
            name: "authors"
            when: viewable == "authors"
	    PropertyChanges { target: item_authors; opacity: 1 }
	    PropertyChanges { target: item_license; opacity: 0 }
	},
	State {
            name: "license"
            when: viewable == "license"
	    PropertyChanges { target: item_authors; opacity: 0 }
	    PropertyChanges { target: item_license; opacity: 1 }
	} ]
    transitions: Transition {
	PropertyAnimation { property: "opacity"; duration: 1000 }
    }
    VerticalScrollDecorator { flickable: about }
}

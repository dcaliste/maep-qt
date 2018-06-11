/*
 * about.qml
 * Copyright (C) Damien Caliste 2013-2018 <dcaliste@free.fr>
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
            text: qsTr("Miscellaneous")
            onClicked: { viewable = "info" }
        }
        MenuItem {
            text: qsTr("License")
            onClicked: { viewable = "license" }
        }
        MenuItem {
            text: qsTr("Authors")
            onClicked: { viewable = "authors" }
        }
    }

    contentHeight: title.height + content.height
    contentWidth: childrenRect.width

    PageHeader { id: title; title: qsTr("About Mæp") }
    Column {
        id: content
        width: parent.width - Theme.horizontalPageMargin
        anchors.top: title.bottom
        anchors.right: parent.right
        spacing: Theme.paddingMedium
        Image {
            id: icon
            source: "file:///usr/share/icons/hicolor/86x86/apps/harbour-maep-qt.png"
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Label {
            id: subtitle
            width: parent.width
            text: qsTr("A small and fast tile map")
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeMedium
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            id: lbl_version
            width: parent.width
            text: qsTr("Version %1 - %2").arg(version).arg(date)
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            id: copyright
            width: parent.width
            text: qsTr("Copyright %1").arg("2009 - 2018")
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
        }
        Button {
            text: "https://github.com/dcaliste/maep-qt"
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: { Qt.openUrlExternally(text) }
        }
        Item {
            width: parent.width
            height: Math.max(item_authors.height, Math.max(item_license.height, item_info.height))
            Item {
                id: item_authors
                opacity: 0
                Behavior on opacity { FadeAnimation{} }
                visible: opacity > 0
                width: parent.width - 2 * Theme.paddingSmall
                height: childrenRect.height
                anchors.horizontalCenter: parent.horizontalCenter
                Label {
                    id: lbl_title_authors
                    text: qsTr("Authors")
                    color: Theme.highlightColor
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
                Behavior on opacity { FadeAnimation{} }
                visible: opacity > 0
                width: parent.width - 2 * Theme.paddingSmall
                height: childrenRect.height
                anchors.horizontalCenter: parent.horizontalCenter
                Label {
                    id: lbl_title_license
                    text: qsTr("License")
                    color: Theme.highlightColor
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
            Item {
                id: item_info
                opacity: 0
                Behavior on opacity { FadeAnimation{} }
                visible: opacity > 0
                width: parent.width - 2 * Theme.paddingSmall
                height: childrenRect.height
                anchors.horizontalCenter: parent.horizontalCenter
                Label {
                    id: lbl_title_bugs
                    text: qsTr("Report bugs")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                    anchors.topMargin: Theme.paddingSmall
                }
                Label {
                    id: lbl_bugs
                    width: parent.width - Theme.paddingMedium * 2
                    text: qsTr("Please report bugs or feature requests via the Mæp " +
                    "bug tracker. This bug tracker can directly be reached via " +
                    "the following link:")
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignJustify
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: lbl_title_bugs.bottom
                }
                Button {
                    id: bt_bugs
                    text: qsTr("Open bug tracker")
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: lbl_bugs.bottom
                    onClicked: { Qt.openUrlExternally("https://github.com/dcaliste/maep-qt/issues") }
                }
                Label {
                    id: lbl_title_donate
                    text: qsTr("Donate")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                    anchors.topMargin: Theme.paddingSmall
                    anchors.top: bt_bugs.bottom
                }
                Label {
                    id: lbl_donate
                    width: parent.width - Theme.paddingMedium * 2
                    text: qsTr("If you like Mæp and want to support its future development " +
                    "please consider donating to the developer. You can either " +
                    "donate via paypal to till@harbaum.org or you can just click " +
                    "the button below which will open the appropriate web page in " +
                    "your browser.")
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignJustify
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: lbl_title_donate.bottom
                }
                Button {
                    id: bt_donate
                    text: qsTr("Donate through Paypal")
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: lbl_donate.bottom
                    onClicked: { Qt.openUrlExternally("https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=7400558") }
                }
            }
        }
    }
    states: [
    State {
        name: "authors"
        when: viewable == "authors"
        PropertyChanges { target: item_authors; opacity: 1 }
        PropertyChanges { target: item_license; opacity: 0 }
        PropertyChanges { target: item_info;    opacity: 0 }
    },
    State {
        name: "license"
        when: viewable == "license"
        PropertyChanges { target: item_authors; opacity: 0 }
        PropertyChanges { target: item_license; opacity: 1 }
        PropertyChanges { target: item_info;    opacity: 0 }
    },
    State {
        name: "info"
        when: viewable == "info"
        PropertyChanges { target: item_authors; opacity: 0 }
        PropertyChanges { target: item_license; opacity: 0 }
        PropertyChanges { target: item_info;    opacity: 1 }
    } ]
    VerticalScrollDecorator { flickable: about }
}

/*
 * Sources.qml
 * Copyright (C) Damien Caliste 2017 <dcaliste@free.fr>
 *
 * main.qml is free software: you can redistribute it and/or modify it
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
import harbour.maep.qt 1.0

Page {
    property GpsMap map

    allowedOrientations: page.allowedOrientations
    
    ListModel {
        id: sourceModel
        ListElement { source: GpsMap.SOURCE_OPENSTREETMAP
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_OPENCYCLEMAP
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_OSM_PUBLIC_TRANSPORT
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_MML_PERUSKARTTA
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_MML_ORTOKUVA
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_MML_TAUSTAKARTTA
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_GOOGLE_STREET
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_VIRTUAL_EARTH_STREET
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_VIRTUAL_EARTH_SATELLITE
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_VIRTUAL_EARTH_HYBRID
                      section: "base tiles" }
        ListElement { source: GpsMap.SOURCE_OPENSEAMAP
                      section: "overlay tiles" }
        //ListElement { source: GpsMap.SOURCE_GOOGLE_TRAFFIC
            //              section: "overlay tiles" }
    }

    SilicaListView {
        id: sourcelist
        anchors.fill: parent

        header: Column {
            width: parent.width
            PageHeader {
                title: "Select a tile source"
            }
            Label {
                width: parent.width - 2 * Theme.paddingLarge
                text: "Long tap to display options"
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        model: sourceModel

        section {
            property: 'section'

            delegate: SectionHeader {
                text: section
                height: Theme.itemSizeExtraSmall
            }
        }

        delegate: ListItem {
            id: listItem
            menu: contextMenu
            contentHeight: Theme.itemSizeMedium
            Label {
                text: map.sourceLabel(source)
                width: parent.width - img.width
                anchors.left: img.right
                anchors.top: parent.top
                anchors.topMargin: Theme.paddingMedium
                horizontalAlignment: Text.AlignHCenter
                color: listItem.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
            Label {
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: map.sourceCopyrightNotice(source)
                anchors.right: parent.right
                anchors.rightMargin: Theme.paddingSmall
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.paddingMedium
            }
            Item {
                id: img
                width: Theme.itemSizeMedium * 1.5
                height: Theme.itemSizeMedium - Theme.paddingSmall
                anchors.left: parent.left
                anchors.leftMargin: Theme.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
                Image {
                    visible: model.section == "base tiles"
                    anchors.fill: parent
                    clip: true; fillMode: Image.Pad
                    source: visible ? map.getCenteredTile(model.source) : ""
                }
                Switch {
                    visible: model.section == "overlay tiles"
                    anchors.fill: parent
                    //icon.clip: true; icon.fillMode: Image.Pad
                    //icon.source: map.getCenteredTile(model.source)
                    checked: map.overlaySource == model.source
                }
            }
            onClicked: {
                if (model.section == "base tiles") {
                    map.source = model.source
                } else {
                    map.overlaySource = (map.overlaySource == model.source) ? GpsMap.SOURCE_NULL : model.source
                }
                pageStack.pop()
            }

            Component {
                id: contextMenu
                ContextMenu {
                    MenuItem {
                        text: "Open map copyright in browser"
                        onClicked: Qt.openUrlExternally(map.sourceCopyrightUrl(source))
                    }
                }
            }
        }
        VerticalScrollDecorator { flickable: sourcelist }
    }
}

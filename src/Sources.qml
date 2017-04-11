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

    SourceModel {
        id: sourceModel
        Component.onCompleted: {
            addPreset(SourceModel.SOURCE_OPENSTREETMAP,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_OPENCYCLEMAP,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_OSM_PUBLIC_TRANSPORT,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_MML_PERUSKARTTA,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_MML_ORTOKUVA,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_MML_TAUSTAKARTTA,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_GOOGLE_STREET,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_VIRTUAL_EARTH_STREET,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_VIRTUAL_EARTH_SATELLITE,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_VIRTUAL_EARTH_HYBRID,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_OPENSEAMAP,
                      SourceModel.SECTION_OVERLAY)
        }
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
                width: parent.width - 2 * Theme.horizontalPageMargin - 2 * Theme.paddingLarge
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
                text: {
                    if (section == SourceModel.SECTION_BASE) {
                        return "base tiles"
                    } else if (section == SourceModel.SECTION_OVERLAY) {
                        return "overlay tiles"
                    } else {
                        return ""
                    }
                }
                height: Theme.itemSizeExtraSmall
            }
        }

        delegate: ListItem {
            menu: contextMenu
            contentHeight: Theme.itemSizeMedium
            Label {
                text: label
                width: parent.width - img.width
                anchors.left: img.right
                anchors.top: parent.top
                anchors.topMargin: Theme.paddingMedium
                horizontalAlignment: Text.AlignHCenter
                color: sourceId == map.source ? Theme.highlightColor : Theme.primaryColor
            }
            Label {
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: copyrightNotice
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin + Theme.paddingSmall
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.paddingMedium
            }
            Item {
                id: img
                width: Theme.itemSizeMedium * 1.5
                height: Theme.itemSizeMedium - Theme.paddingSmall
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin + Theme.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
                Image {
                    visible: section == SourceModel.SECTION_BASE
                    anchors.fill: parent
                    clip: true; fillMode: Image.Pad
                    source: visible ? map.getCenteredTile(sourceId) : ""
                }
                Switch {
                    visible: section == SourceModel.SECTION_OVERLAY
                    anchors.fill: parent
                    //icon.clip: true; icon.fillMode: Image.Pad
                    //icon.source: map.getCenteredTile(model.source)
                    checked: map.overlaySource == sourceId
                }
            }
            onClicked: {
                if (section == SourceModel.SECTION_BASE) {
                    map.source = sourceId
                } else if (section == SourceModel.SECTION_OVERLAY) {
                    map.overlaySource = (map.overlaySource == sourceId) ? SourceModel.SOURCE_NULL : sourceId
                }
                pageStack.pop()
            }

            Component {
                id: contextMenu
                ContextMenu {
                    MenuItem {
                        text: "Open map copyright in browser"
                        onClicked: Qt.openUrlExternally(copyrightUrl)
                    }
                }
            }
        }
        VerticalScrollDecorator { flickable: sourcelist }
    }
}

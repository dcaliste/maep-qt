/*
 * main.qml
 * Copyright (C) Damien Caliste 2013-2022 <dcaliste@free.fr>
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

import Nemo.Notifications 1.0
import QtPositioning 5.2

ApplicationWindow
{
    id: main
    initialPage: page
    cover: coverPage
    _defaultPageOrientations: Orientation.All

    SourceModel {
        id: sources
        Component.onCompleted: {
            addPreset(SourceModel.SOURCE_OPENSTREETMAP,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_OPENCYCLEMAP,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_OSM_PUBLIC_TRANSPORT,
                      SourceModel.SECTION_BASE)
            addPreset(SourceModel.SOURCE_HIKE_AND_BIKE,
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
            addPreset(SourceModel.SOURCE_HILL_SHADING,
                      SourceModel.SECTION_OVERLAY)
        }
    }

    Page {
        id: page
        allowedOrientations: map.screen_rotation ? Orientation.All : pageStack.currentOrientation

        function importTrack() {
            var obj = pageStack.animatorPush(trackopen)
            obj.pageCompleted.connect(function(dialog) {
                dialog.accepted.connect(function() {
                    if (!dialog.track.isEmpty()) {
                        map.track_capture = false // disable track capture to avoid
                                                  // adding new point from current
                                                  // location to old tracks.
                        map.setTrack(dialog.track)
                    }
                })
            })
        }

        Item {
            anchors.top: topPart.bottom
            anchors.topMargin: topPart.contentY < 0 ? -topPart.contentY : 0
            anchors.bottom: parent.bottom
            width: page.width

            GpsMap {
                id: map
                property Conf conf: Conf {  }
                property int track_autosave_rate: conf.getInt("track_autosave_period", 300)
                property int track_metric_accuracy: conf.getInt("track_metric_accuracy", 30)
                anchors.fill: parent
                onSearchRequest: { header.searchFocus = true }
                onWikiEntryChanged: { pageStack.animatorPush(wiki) }
                onWikiStatusChanged: { wikicheck.checked = status }
                onSearchResults: header.searchResults(search_results)
                onTrack_captureChanged: {
                    if (!track_capture && track) { track.finalizeSegment() }
                }
                onTrack_autosave_rateChanged: {
                    if (track) { track.autosavePeriod = track_autosave_rate }
                    conf.setInt("track_autosave_period", track_autosave_rate)
                }
                onTrack_metric_accuracyChanged: {
                    if (track) { track.metricAccuracy = track_metric_accuracy }
                    conf.setInt("track_metric_accuracy", track_metric_accuracy)
                }
                onTrackChanged: if (track) {
                    track.autosavePeriod = track_autosave_rate
                    track.metricAccuracy = track_metric_accuracy
                }
            }
            OpacityRampEffect {
                enabled: map_controls.visible
                offset: 1. - map_controls.height / map.height
                slope: map.height / map_controls.height
                direction: 2
                sourceItem: map
            }
            Row {
                id: map_controls
                anchors.bottom: parent.bottom
                width: parent.width
                height: Theme.itemSizeSmall
                z: map.z + 1
                visible: !Qt.inputMethod.visible
                IconButton {
                    id: zoomout
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "file:///usr/share/harbour-maep-qt/icon-camera-zoom-wide.png"
                    enabled: map.canZoomOut
                    onClicked: { map.zoomOut() }
                }
                IconButton {
                    id: zoomin
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "file:///usr/share/harbour-maep-qt/icon-camera-zoom-tele.png"
                    enabled: map.canZoomIn
                    onClicked: { map.zoomIn() }
                }
                Button {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - zoomout.width - zoomin.width - autocenter.width
                    text: map.sourceLabel
                    /*font.pixelSize: Theme.fontSizeSmall*/
                    onClicked: { pageStack.animatorPush("Sources.qml", {"map": map, "sources": sources}) }
                }
                IconButton {
                    id: autocenter
                    icon.source: "image://theme/icon-m-gps"
                    anchors.verticalCenter: parent.verticalCenter
                    enabled: map.gps_coordinate.latitude <= 90 && map.gps_coordinate.latitude >= -90
                    highlighted: map.auto_center
                    onClicked: { map.auto_center = !map.auto_center }
                }
            }
        }
        SilicaFlickable {
            id: topPart
            anchors.top: parent.top
            width: page.width
            height: Math.min(Theme.itemSizeMedium + 5 * Theme.itemSizeSmall,
                             contentHeight)
            contentHeight: header.height
            clip: page.status != PageStatus.Active || !mainMenu.active
            flickableDirection: Flickable.VerticalFlick
            z: -1

            PullDownMenu {
                id: mainMenu
                MenuItem {
                    text: qsTr("About Mæp")
                    onClicked: pageStack.push(aboutpage)
                }
                MenuItem {
                    text: qsTr("Settings")
                    onClicked: pageStack.push(settingspage)
                }
                MenuItem {
                    TextSwitch {
                        id: orientationcheck
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Screen rotation")
                        checked: map.screen_rotation
                        onCheckedChanged: { map.screen_rotation = checked }
                    }
                    onClicked: {orientationcheck.checked = !orientationcheck.checked}
                }
                MenuItem {
                    TextSwitch {
                        id: wikicheck
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Wikipedia")
                        checked: map.wiki_status
                        onCheckedChanged: { map.wiki_status = checked }
                    }
                    onClicked: {wikicheck.checked = !wikicheck.checked}
                }
                MenuItem {
                    TextSwitch {
                        id: trackcheck
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Track capture")
                        checked: map.track_capture
                        onCheckedChanged: { map.track_capture = checked }
                    }
                    onClicked: {trackcheck.checked = !trackcheck.checked}
                }
                MenuItem {
                    text: qsTr("import track from device")
                    onClicked: page.importTrack()
                }
                /*MenuItem {
                    text: "Next action"
                    onClicked: header.nextAction()
                }*/
            }
            Header {
                id: header
                width: parent.width
            }
        }
    }

    CoverBackground {
        id: coverPage
        property bool active: status == Cover.Active
        GpsMapCover {
            id: coverMap
            width: parent.width
            height: parent.height
            map: map
            status: coverPage.active
        }
        OpacityRampEffect {
            enabled: map.track
            offset: 1. - (coverTrack.childrenRect.height + Theme.paddingLarge) / coverMap.height
            slope: coverMap.height / Theme.paddingLarge / 3.
            direction: 3
            sourceItem: coverMap
        }
        Item {
            visible: map.track
            width: parent.width
            
            Column {
                id: coverTrack
                anchors.top: parent.top
                width: parent.width
                Label {
                    function duration(time) {
                        if (time < 60) {
                            return qsTr("%1 s").arg(time)
                        } else if (time < 3600) {
                            var m = Math.floor(time / 60)
                            return  qsTr("%1 min").arg(m)
                        } else {
                            var h = Math.floor(time / 3600)
                            var m = Math.floor((time - h * 3600) / 60)
                            return qsTr("%1 h %2").arg(h).arg(m)
                        }
                    }
                    function length(lg) {
                        if (lg >= 1000) {
                            return qsTr("%L1 km").arg((lg / 1000).toFixed(1))
                        } else {
                            return qsTr("%1 m").arg(lg.toFixed(0))
                        }
                    }
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeSmall
                    text: if (map.track && map.track.duration > 0) { length(map.track.length) + " (" + duration(map.track.duration) + ")"} else qsTr("no accurate data")
                }
                Label {
                    function speed(length, time) {
                        if (time > 0) {
                            return qsTr("%1 km/h").arg((length / time * 3.6).toFixed(2))
                        } else {
                            return ""
                        }
                    }
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeSmall
                    text: if (map.track) { speed(map.track.length, map.track.duration) } else ""
                }
            }
            Rectangle {
                width: Theme.paddingSmall
                height: coverTrack.height
                color: map.trackColor
                radius: Theme.paddingSmall / 2
            }
        }
    }

    CoverActionList {
        enabled: true
        iconBackground: true
        CoverAction {
            iconSource: "file:///usr/share/harbour-maep-qt/icon-cover-remove.png"
            onTriggered: map.zoomOut()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-new"
            onTriggered: map.zoomIn()
        }
    }

    Component {
        id: wiki

        Page {
            allowedOrientations: page.allowedOrientations
            PageHeader {
                id: wikititle
                title: map.wiki_entry.title
            }
            Item {
                id: thumbnail
                visible: (map.wiki_entry.thumbnail != "")
                width: wikiimg.width + Theme.paddingSmall * 2
                height: wikiimg.height + Theme.paddingSmall * 2
                anchors.top: wikititle.bottom
                anchors.topMargin: page.isPortrait ? Theme.paddingLarge : Theme.paddingSmall
                anchors.horizontalCenter: page.isPortrait ? wikititle.horizontalCenter : undefined
                anchors.left: page.isPortrait ? undefined : parent.left
                anchors.leftMargin: page.isPortrait ? undefined : Theme.paddingLarge
                Rectangle {
                    id: frame
                    color: Theme.secondaryColor
                    radius: Theme.paddingSmall
                    anchors.fill: parent
                    opacity: 0.5
                }
                Image {
                    id: wikiimg
                    anchors.centerIn: frame
                    source: map.wiki_entry.thumbnail
                    sourceSize.width: 360
                    sourceSize.height: 360
                }
            }
            Label {
                property real dist: map.coordinate.distanceTo(map.wiki_entry.coordinate)
                property string at: dist >= 1000 ? qsTr("at %L1 km").arg((dist / 1000).toFixed(1)) : qsTr("at %1 m").arg(dist.toFixed(0))
                id: coordinates
                anchors.top: page.isPortrait ? thumbnail.bottom : undefined
                anchors.right: parent.right
                anchors.left: page.isPortrait ? parent.left : thumbnail.right
                anchors.rightMargin: Theme.paddingMedium
                anchors.verticalCenter: page.isPortrait ? undefined : thumbnail.verticalCenter
                text: qsTr("coordinates: %1\n").arg(map.wiki_entry.coordinateToString()) + at
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                horizontalAlignment: Text.AlignRight
            }
            Label {
                id: body
                text: map.wiki_entry.summary
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                width: parent.width - Theme.paddingMedium * 2
                horizontalAlignment: Text.AlignJustify
                anchors.topMargin: Theme.paddingLarge
                anchors.horizontalCenter: wikititle.horizontalCenter
                anchors.top: page.isPortrait ? coordinates.bottom : thumbnail.bottom
            }
            Button {
                text: qsTr("Open Wikipedia page")
                /*onClicked: { pageStack.pushAttached(wikipedia); pageStack.navigateForward() }*/
                onClicked: { Qt.openUrlExternally(map.wiki_entry.url) }
                anchors.top: body.bottom
                anchors.topMargin: page.isPortrait ? Theme.paddingLarge : Theme.paddingSmall
                anchors.horizontalCenter: wikititle.horizontalCenter
            }
            Label {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.rightMargin: Theme.paddingMedium
                font.pixelSize: Theme.fontSizeExtraSmall
                font.italic: true
                color: Theme.secondaryColor
                text: qsTr("source: geonames.org")
            }
        }
    }

    /*Component {
        id: wikipedia
        
        Page {
            allowedOrientations: page.allowedOrientations
            PageHeader {
                id: wikititle
                title: map.wiki_entry.title
            }
            SilicaWebView {
                id: webView
                anchors {
                    top: wikititle.bottom
                    bottom: parent.bottom
                }
                width: page.width
                opacity: 0
                onLoadingChanged: {
                    switch (loadRequest.status)
                    {
                        case WebView.LoadSucceededStatus:
                        opacity = 1
                        break
                        case WebView.LoadFailedStatus:
                        opacity = 0
                        viewPlaceHolder.errorString = loadRequest.errorString
                        break
                        default:
                        opacity = 0
                        break
                    }
                }

                FadeAnimation on opacity {}
            }
            ViewPlaceholder {
                id: viewPlaceHolder
                property string errorString

                enabled: webView.opacity == 0
                text: webView.loading ? "Loading" : "Web content load error: " + errorString
                hintText: map.wiki_entry.url
            }
            Component.onCompleted: {
                webView.url = map.wiki_entry.url
            }
        }
    }*/

    Component {
        id: tracksave

        Dialog {
            property Track track
            property Conf conf:  Conf {  }
            
            function save(url) { track.toFile(url); accept() }
            function overwrite(url) {
                console.log("overwritting " + url)
                console.log("export track" + track)
                remorse.execute(qsTr("Overwrite file?"), function() { save(url) })
            }
            onOpened: { var url = conf.getString("track_path")
                if (url.length > 0) {
                    chooser.folder = url.substring(0, url.lastIndexOf("/"))
                } else { chooser.folder = StandardPaths.documents } }
            onAccepted: { var url = chooser.getEntry();
                if (url.length > 0) {track.toFile(url)} }
            RemorsePopup { id: remorse }
            FileChooser {
                id: chooser
                anchors.fill: parent
                saveMode: true
                title: DialogHeader { title: qsTr("Save current track") }
                saveText: track.path.substring(track.path.lastIndexOf("/") + 1)
                onSelectionChanged: { overwrite(selection) }
                onEntryChanged: { save(entry) }
            }
        }
    }

    Component {
        id: trackopen

        Dialog {
            property Track track: Track { onFileError: { console.log(errorMsg);
                notification.previewBody = errorMsg;
                notification.publish() } }
            property Conf conf:  Conf {  }
            
            function load(url) {
                if (track.set(url)) { accept() }
            }
            onOpened: { var url = conf.getString("track_path")
                if (url.length > 0) {
                    chooser.folder = url.substring(0, url.lastIndexOf("/"))
                } else { chooser.folder = StandardPaths.documents } }
            FileChooser {
                id: chooser
                anchors.fill: parent
                title: DialogHeader { title: qsTr("Select a track file") }
                onSelectionChanged: { load(selection) }
            }
            
            Notification {
                id: notification
                previewSummary: qsTr("Loading a track file")
            }
        }
    }

    Component {
        id: aboutpage
        Page {
            About {
                anchors.fill: parent
                version: map.version
                date: map.compilation_date
                authors: map.authors
                license: map.license
            }
        }
    }

    Component {
        id: settingspage
        Page {
            Settings {
                anchors.fill: parent
            }
        }
    }
}

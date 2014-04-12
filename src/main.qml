/*
 * main.qml
 * Copyright (C) Damien Caliste 2013-2014 <dcaliste@free.fr>
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
import Maep 1.0

import QtPositioning 5.0

ApplicationWindow
{
    id: main
    initialPage: page
    cover: coverPage
    
    Page {
        id: page
	allowedOrientations: map.screen_rotation ? Orientation.All : main.deviceOrientation

        function importTrack() {
            var dialog = pageStack.push(trackopen)
	    dialog.accepted.connect(
                function() {
                    if (!dialog.track.isEmpty()) {
                        map.track_capture = false // disable track capture to avoid
                                                  // adding new point from current
                                                  // location to old tracks.
                        map.setTrack(dialog.track)
                    }
                } )
        }

        Drawer {
 	    id: drawer
            anchors.top: header.bottom
            width: page.width
            height: page.height - header.height

            dock: page.isPortrait ? Dock.Top : Dock.Left

            background: PlaceView {
                id: placeview
                anchors.fill: parent
                currentPlace: map.coordinate
                onSelection: {
                    drawer.open = false
                    map.setLookAt(lat, lon)
                    header.searchText = text
                }
            }

            GpsMap {
                id: map
                property Conf conf: Conf {  }
                property int track_autosave_rate: conf.getInt("track_autosave_period", 300)
                property int track_metric_accuracy: conf.getInt("track_metric_accuracy", 30)
                /*anchors.top: header.bottom
                  width: page.width
                  height: page.height - header.height*/
	        anchors.fill: parent
                onSearchRequest: { header.searchFocus = true }
                onWikiEntryChanged: { pageStack.push(wiki) }
	        onWikiStatusChanged: { wikicheck.checked = status }
                onSearchResults: {
                    header.searchResults(search_results)
		    if (search_results.length > 0) { placeview.model = search_results
                                                     drawer.open = true }
		}
	        Behavior on opacity {
                    NumberAnimation {easing.type: Easing.InOutCubic}
                }
		onFocusChanged: { if (focus) { drawer.open = false } }
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
                offset: 1. - Theme.itemSizeMedium / map.height
                slope: map.height / Theme.itemSizeMedium
                direction: 2
                sourceItem: map
                clampMax: map.opacity
            }
	    Row {
                id: map_controls
		anchors.bottom: parent.bottom
		width: parent.width
		height: Theme.itemSizeMedium
		z: map.z + 1
                anchors.bottomMargin: -Theme.paddingMedium
                visible: !header.searchFocus && !header.trackFocus && !drawer.open
                opacity: map.opacity
		IconButton {
		    id: zoomout
		    icon.source: "image://theme/icon-camera-zoom-wide"
		    onClicked: { map.zoomOut() }
		}
                IconButton {
		    id: zoomin
                    icon.source: "image://theme/icon-camera-zoom-tele"
		    onClicked: { map.zoomIn() }
                }
                Button {
		    width: parent.width - zoomout.width - zoomin.width - autocenter.width
                    text: map.sourceLabel(map.source)
		    /*font.pixelSize: Theme.fontSizeSmall*/
                    onClicked: { pageStack.push(sources) }
                }
                IconButton {
                    id: autocenter
                    icon.source: "image://theme/icon-m-gps"
                    anchors.rightMargin: Theme.paddingSmall
                    anchors.leftMargin: Theme.paddingSmall
		    enabled: map.gps_coordinate.latitude <= 90 && map.gps_coordinate.latitude >= -90
		    highlighted: map.auto_center
		    onClicked: { map.auto_center = !map.auto_center }
                }
	    }
	}
        Header {
            id: header
            anchors.top: parent.top
            width: page.width

            PullDownMenu {
                MenuItem {
                    text: "About Mæp"
                    onClicked: pageStack.push(aboutpage)
                }
                MenuItem {
                    text: "Settings"
                    onClicked: pageStack.push(settingspage)
                }
                MenuItem {
                    TextSwitch {
                        id: orientationcheck
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Screen rotation"
                        checked: map.screen_rotation
                        onCheckedChanged: { map.screen_rotation = checked }
                    }
                    onClicked: {orientationcheck.checked = !orientationcheck.checked}
                }
	        MenuItem {
	            TextSwitch {
		        id: wikicheck
		        anchors.horizontalCenter: parent.horizontalCenter
		        text: "Wikipedia"
		        checked: map.wiki_status
                        onCheckedChanged: { map.wiki_status = checked }
	            }
                    onClicked: {wikicheck.checked = !wikicheck.checked}
	        }
                MenuItem {
                    TextSwitch {
                        id: trackcheck
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Track capture"
                        checked: map.track_capture
                        onCheckedChanged: { map.track_capture = checked }
                    }
                    onClicked: {trackcheck.checked = !trackcheck.checked}
                }
                MenuItem {
                    text: "Next action"
                    onClicked: header.nextAction()
                }
	        onActiveChanged: active ? map.opacity = Theme.highlightBackgroundOpacity : map.opacity = 1
            }

            resultVisible: drawer.open
            onSearchRequest: {
                drawer.open = false
                placeview.model = null
                map.focus = true
                map.setSearchRequest(text)
            }
            onShowResults: drawer.open = status
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
        Column {
            id: coverTrack
            visible: map.track
            anchors.top: parent.top
            width: parent.width
            Label {
                function duration(time) {
                    if (time < 60) {
                        return time + " s"
                    } else if (time < 3600) {
                        var m = Math.floor(time / 60)
                        return  m + " min"
                    } else {
                        var h = Math.floor(time / 3600)
                        var m = Math.floor((time - h * 3600) / 60)
                        return h + " h " + m
                    }
                }
                function length(lg) {
                    if (lg >= 1000) {
                        return (lg / 1000).toFixed(1) + " km"
                    } else {
                        return lg.toFixed(0) + " m"
                    }
                }
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeSmall
	        text: if (map.track) { length(map.track.length) + " (" + duration(map.track.duration) + ")"} else ""
            }
            Label {
                function speed(length, time) {
                    if (time > 0) {
                        return (length / time * 3.6).toFixed(2) + " km/h"
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
            color: "#EA0000"
            opacity: 0.6
            radius: 2
        }
    }

    CoverActionList {
        enabled: true
        iconBackground: true
        CoverAction {
            iconSource: "image://theme/icon-camera-zoom-wide"
            onTriggered: map.zoomOut()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-new"
            onTriggered: map.zoomIn()
        }
    }

    ListModel {
        id: sourceModel
	ListElement { source: GpsMap.SOURCE_OPENSTREETMAP }
	ListElement { source: GpsMap.SOURCE_OPENSTREETMAP_RENDERER }
	ListElement { source: GpsMap.SOURCE_OPENCYCLEMAP }
        ListElement { source: GpsMap.SOURCE_OSM_PUBLIC_TRANSPORT }
        ListElement { source: GpsMap.SOURCE_GOOGLE_STREET }
        ListElement { source: GpsMap.SOURCE_VIRTUAL_EARTH_STREET }
        ListElement { source: GpsMap.SOURCE_VIRTUAL_EARTH_SATELLITE }
        ListElement { source: GpsMap.SOURCE_VIRTUAL_EARTH_HYBRID }
        //ListElement { source: GpsMap.SOURCE_OPENSEAMAP }
    }

    Component {
        id: sources

        Dialog {
	    id: sourcedialog
            allowedOrientations: page.allowedOrientations
            
            SilicaListView {
                id: sourcelist
                anchors.fill: parent

                header: Column {
                    width: parent.width
                    DialogHeader {
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
                    Image { id: img
                            clip: true; fillMode: Image.Pad
                            width: Theme.itemSizeMedium * 1.5
                            height: Theme.itemSizeMedium - Theme.paddingSmall
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.paddingSmall
                            anchors.verticalCenter: parent.verticalCenter
                            onVisibleChanged: {
                                if (visible) { source = map.getCenteredTile(model.source) } }
                          }
                    onClicked: { map.source = model.source; sourcedialog.accept() }

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
		property string at: dist >= 1000 ? "at " + (dist / 1000).toFixed(1) + " km" : "at " + dist.toFixed(0) + " m"
		id: coordinates
		anchors.top: page.isPortrait ? thumbnail.bottom : undefined
		anchors.right: parent.right
		anchors.left: page.isPortrait ? parent.left : thumbnail.right
		anchors.rightMargin: Theme.paddingMedium
		anchors.verticalCenter: page.isPortrait ? undefined : thumbnail.verticalCenter
		text: "coordinates: " + map.wiki_entry.coordinateToString() + "\n" + at
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
		text: "Open Wikipedia page"
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
		text: "source: geonames.org"
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
                remorse.execute("Overwrite file ?", function() { save(url) })
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
		title: DialogHeader { title: "Save current track" }
		onSelectionChanged: { overwrite(selection) }
		onEntryChanged: { save(entry) }
	    }
	}
    }

    Component {
	id: trackopen

	Dialog {
	    property Track track: Track { onFileError: { console.log(errorMsg);
                                                         banner.text = errorMsg;
                                                         banner.active = true } }
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
		title: DialogHeader { title: "Select a track file" }
		onSelectionChanged: { load(selection) }
	    }
	    Notification {
		id: banner
		anchors.fill: parent
	    }
	}
    }

    Component {
        id: aboutpage
        Page {
            anchors.fill: parent
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
            anchors.fill: parent
            Settings {
                anchors.fill: parent
            }
        }
    }
}

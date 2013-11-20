/*
 * main.qml
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
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
import QtWebKit 3.0
import QtPositioning 5.0
import Qt.labs.folderlistmodel 1.0

ApplicationWindow
{
    id: main
    initialPage: page
    cover: coverPage
    
    Page {
        id: page
	allowedOrientations: map.screen_rotation ? Orientation.All : main.deviceOrientation

        // To enable PullDownMenu, place our content in a SilicaFlickable
        SilicaFlickable {
            id: header
            anchors.top: parent.top

            // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
            PullDownMenu {
                MenuItem {
                    text: "About Mæp"
                    onClicked: pageStack.push(aboutpage)
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
                    text: "Import track"
                    font.pixelSize: Theme.fontSizeSmall
	            color: Theme.secondaryHighlightColor
                    onClicked: { var dialog = pageStack.push(trackopen)
		                 dialog.accepted.connect(
                                     function() {
                                         if (!dialog.track.isEmpty()) {
                                             map.setTrack(dialog.track) }
                                     } ) }
                }
                MenuItem {
                    text: "Export track"
                    visible: map.track != undefined
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    onClicked: pageStack.push(tracksave, { track: map.track })
                }
                MenuItem {
                    text: "Clear track"
                    visible: map.track != undefined
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    onClicked: map.setTrack()
                }
		onActiveChanged: { active ? map.opacity = Theme.highlightBackgroundOpacity : map.opacity = 1 }
            }

            // Tell SilicaFlickable the height of its content.
            width: page.width
	    height: Theme.itemSizeMedium //childrenRect.height
            contentHeight: childrenRect.height

            Row {
                width: parent.width
	        spacing: 0 /*Theme.paddingSmall*/
                TextField {
                    id: search
                    width: parent.width - maep.width
                    placeholderText: "Enter a place name"
	            label: "Place search"
		    anchors.verticalCenter: parent.verticalCenter
		    EnterKey.text: "search"
		    EnterKey.onClicked: {
		        busy.running = true
		        map.focus = true
		        drawer.open = false
			placeview.model = null
                        map.setSearchRequest(text)
		    }
		    onFocusChanged: { if (focus) { selectAll() } }
                }
	        BusyIndicator {
	   	    id: busy
                    running: false
                    visible: true
                    size: BusyIndicatorSize.Small
                    anchors.verticalCenter: parent.verticalCenter
                }
                PageHeader {
                    id: maep
                    width: 130
                    title: "Mæp"
                }
            }
        }
        Drawer {
 	    id: drawer
            z: -1
            anchors.top: header.bottom
            width: page.width
            height: page.height - header.height

            dock: page.isPortrait ? Dock.Top : Dock.Left

            background: placeview

            GpsMap {
                id: map
                /*anchors.top: header.bottom
                  width: page.width
                  height: page.height - header.height*/
	        anchors.fill: parent
                onSearchRequest: { search.focus = true; search.label = "Place search" }
                onWikiEntryChanged: { pageStack.push(wiki) }
	        onWikiStatusChanged: { wikicheck.checked = status }
                onSearchResults: { search.label = search_results.length + " place(s) found"
			           busy.running = false
				   placeview.model = search_results
			           drawer.open = true }
	        Behavior on opacity {
                    FadeAnimation {}
                }
		onFocusChanged: { if (focus) { drawer.open = false } }
            }
        }

        SilicaListView {
            id: placeview
            anchors.fill: parent
            /*header: DialogHeader {
              title: "Select a place"
              }*/
	    onModelChanged: { console.log("hey, model changed"); console.log(map.search_results.count()); }

	    PullDownMenu {
	        MenuItem {
		    text: "Cancel search"
		    font.pixelSize: Theme.fontSizeSmall
		    onClicked: { drawer.open = false }
	        }
	    }
	    PushUpMenu {
                MenuItem {
                    text: "Cancel search"
		    font.pixelSize: Theme.fontSizeSmall
                    onClicked: { drawer.open = false }
                }
            }

	    ViewPlaceholder {
                enabled: placeview.count == 0
                text: "No result"
            }

            delegate: ListItem {
		
		contentHeight: Theme.itemSizeSmall * 0.75
		Label {
                    text: model.name + ", " + model.country
		    font.pixelSize: Theme.fontSizeSmall
		    anchors.fill: parent
		    color: highlighted ? Theme.highlightColor : Theme.primaryColor
		}
		Label {
		    property real dist: map.coordinate.distanceTo(model.coordinate)
		    font.pixelSize: Theme.fontSizeExtraSmall
		    text: dist >= 1000 ? "at " + (dist / 1000).toFixed(1) + " km" : "at " + dist.toFixed(0) + " m"
		    color: Theme.secondaryColor
		    anchors.right: parent.right
		    anchors.bottom: parent.bottom
		}
	        onClicked: { search.text = model.name
	    		     drawer.open = false
			     map.setLookAt(model.coordinate.latitude, model.coordinate.longitude) } 
            }
	    VerticalScrollDecorator { flickable: placeview }
        }
    }

    CoverBackground {
	id: coverPage
	property bool active: status == Cover.Active
        GpsMapCover {
            width: parent.width
            height: parent.height
            map: map
	    status: coverPage.active
        }
    }

    CoverActionList {
        enabled: true
        iconBackground: true
        CoverAction {
            iconSource: "image://theme/icon-camera-zoom-wide" /*"image://theme/icon-m-remove"*/
            onTriggered: map.zoomOut()
        }
        CoverAction {
            iconSource: "image://theme/icon-camera-zoom-tele" /*"image://theme/icon-m-add"*/
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
		text: "View Wikipedia page"
		onClicked: { pageStack.pushAttached(wikipedia); pageStack.navigateForward() }
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

    Component {
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
    }

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
	    RemorsePopup { id: remorse }
	    FileChooser {
		id: chooser
		anchors.fill: parent
                saveMode: true
		title: DialogHeader { title: "Save current track" }
		onSelectionChanged: { overwrite(selection) }
		onEntryChanged: { save(selection) }
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
}
